/******************************************************************************
Copyright 2015 RAPP

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

  Author: Athanassios Kintsakis
  contact: akintsakis@issel.ee.auth.gr

 ******************************************************************************/
#include <knowrob_wrapper/knowrob_wrapper.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sstream>
#include <ros/package.h>
#include <fstream>

std::string ontologyDefaultPath = std::string("currentOntologyVersion.owl");

/**  
 * @brief Default constructor 
 */
KnowrobWrapper::KnowrobWrapper(ros::NodeHandle nh) : nh_(nh) {
    mysql_write_client = nh_.serviceClient<rapp_platform_ros_communications::writeDataSrv>("/rapp/rapp_mysql_wrapper/tbl_user_write_data");
    mysql_fetch_client = nh_.serviceClient<rapp_platform_ros_communications::fetchDataSrv>("/rapp/rapp_mysql_wrapper/tbl_user_fetch_data");
    mysql_update_client = nh_.serviceClient<rapp_platform_ros_communications::updateDataSrv>("/rapp/rapp_mysql_wrapper/tbl_user_update_data");
}

void KnowrobWrapper::dump_ontology_now() {
    rapp_platform_ros_communications::ontologyLoadDumpSrv::Request dmp;
    dmp.file_url = ontologyDefaultPath;
    KnowrobWrapper::dumpOntologyQuery(dmp);
}

/** 
 * @brief Converts integer to string 
 * @param a [int] The input integer 
 * @return out [string] The output string 
 */
std::string intToString(int a) {
    std::ostringstream temp;
    temp << a;
    return temp.str();
}

/** 
 * @brief Keeps only the final folder or file from a path
 * @param str [string&] The input path 
 * @return out [string] The output filename 
 */
std::string SplitFilename(const std::string& str) {
    size_t found;
    found = str.find_last_of("/\\");
    return str.substr(0, found);
}

/** 
 * @brief Checks if path exists
 * @param fileName [char*] The input path 
 * @return out [bool] True if file exists 
 */
bool checkIfFileExists(const char *fileName) {
    std::ifstream infile(fileName);
    return infile.good();
}

/** 
 * @brief Splits string by delimiter
 * @param str [string] The input string 
 * @param sep [string] The delimiter
 * @return arr [std::vector<std::string>] A vector with the string parts as splitted by the delimiter 
 */
std::vector<std::string> split(std::string str, std::string sep) {
    char* cstr = const_cast<char*> (str.c_str());
    char* current;
    std::vector<std::string> arr;
    current = strtok(cstr, sep.c_str());
    while (current != NULL) {
        arr.push_back(current);
        current = strtok(NULL, sep.c_str());
    }
    return arr;
}

/** 
 * @brief Returns the ontology alias of the user
 * @param user_id [string] The username of the user 
 * @return ontology_alias [string] The ontology_alias of the user or possible error
 */
std::string KnowrobWrapper::get_ontology_alias(std::string user_id) {
    try {
        std::string ontology_alias;
        rapp_platform_ros_communications::fetchDataSrv srv;
        rapp_platform_ros_communications::StringArrayMsg ros_string_array;
        std::vector<std::string> req_cols;
        req_cols.push_back("ontology_alias");
        ros_string_array.s.push_back("username");
        ros_string_array.s.push_back(user_id);
        srv.request.req_cols = req_cols;
        srv.request.where_data.push_back(ros_string_array);
        mysql_fetch_client.call(srv);
        if (srv.response.success.data != true) {
            throw std::string(std::string("FAIL") + srv.response.trace[0]);
        } else {
            if (srv.response.res_data.size() < 1) {
                throw std::string("FAIL: User not found, incorrect username?");
            }
            ontology_alias = srv.response.res_data[0].s[0];
            if (ontology_alias == std::string("None")) {
                ontology_alias = create_ontology_alias_for_new_user(user_id);
            }
        }
        return ontology_alias;
    } catch (std::string error) {
        return error;
    }

}

/** 
 * @brief Creates a new ontology alias for a user
 * @param user_id [string] The username of the user 
 * @return ontology_alias [string] The ontology_alias of the user or possible error
 */
std::string KnowrobWrapper::create_ontology_alias_for_new_user(std::string user_id) {
    std::string ontology_alias;
    std::vector<std::string> instance_name;
    std::string query = std::string("rdf_instance_from_class(knowrob:'Person") + std::string("',A)");
    json_prolog::PrologQueryProxy results = pl.query(query.c_str());

    char status = results.getStatus();
    if (status == 0) {
        ontology_alias = std::string("User was uninitialized (had no ontology alias), and initilization failed on ontology level");
        return ontology_alias;
    }

    for (json_prolog::PrologQueryProxy::iterator it = results.begin();
            it != results.end(); it++) {
        json_prolog::PrologBindings bdg = *it;
        instance_name.push_back(bdg["A"]);
    }

    ontology_alias = instance_name[0];
    std::vector<std::string> splitted = split(ontology_alias, std::string("#"));
    ontology_alias = splitted[1];

    rapp_platform_ros_communications::updateDataSrv srv;
    srv.request.set_cols.push_back("ontology_alias='" + std::string(ontology_alias) + std::string("'"));
    rapp_platform_ros_communications::StringArrayMsg ros_string_array;
    ros_string_array.s.push_back(std::string("username"));
    ros_string_array.s.push_back(user_id);
    srv.request.where_data.push_back(ros_string_array);
    mysql_update_client.call(srv);
    if (srv.response.success.data != true) {
        ontology_alias = srv.response.trace[0];
        query = std::string("rdf_retractall(knowrob:'") + ontology_alias + std::string("',rdf:type,knowrob:'Person") + std::string("')");
        results = pl.query(query.c_str());
        status = results.getStatus();
        if (status == 0) {
            ontology_alias = ontology_alias + std::string(" DB operation failed.. removing instance from ontology also failed...");
        } else if (status == 3) {
            ontology_alias = ontology_alias + std::string(" DB operation failed..Remove from ontology Success");
        }
        std::string("FAIL") + ontology_alias;
        return ontology_alias;
    }
    KnowrobWrapper::dump_ontology_now();
    return ontology_alias;
}

/** 
 * @brief Implements the record_user_cognitive_tests_performance ROS service 
 * @param req [rapp_platform_ros_communications::recordUserPerformanceCognitiveTestsSrv::Request&] The ROS service request 
 * @return res [rapp_platform_ros_communications::recordUserPerformanceCognitiveTestsSrv::Response&] The ROS service response 
 */
rapp_platform_ros_communications::recordUserPerformanceCognitiveTestsSrv::Response KnowrobWrapper::record_user_cognitive_tests_performance(rapp_platform_ros_communications::recordUserPerformanceCognitiveTestsSrv::Request req) {
    rapp_platform_ros_communications::recordUserPerformanceCognitiveTestsSrv::Response res;
    if (req.test == std::string("") || req.score < 0 || req.score > 100 || req.timestamp < 1 || req.patient_ontology_alias == std::string("") || req.test == std::string("")) {
        res.success = false;
        res.trace.push_back("Error, one or more arguments not provided or out of range. Test score is >=0 and <=100 and timestamp is positive integers");
        res.error = std::string("Error, one or more arguments not provided or out of range. Test score is >=0 and <=100 and timestamp is positive integers");
        return res;
    }
    std::string timestamp = intToString(req.timestamp);
    std::string score = intToString(req.score);

    std::string query = std::string("cognitiveTestPerformed(B,knowrob:'") + req.patient_ontology_alias + std::string("',knowrob:'") + req.test + std::string("','") + timestamp + std::string("','") + score + std::string("',knowrob:'Person',knowrob:'CognitiveTestPerformed')");
    query = std::string("cognitiveTestPerformed(B,knowrob:'") + req.patient_ontology_alias + std::string("',knowrob:'") + req.test + std::string("','") + timestamp + std::string("','") + score + std::string("',knowrob:'Person',knowrob:'CognitiveTestPerformed')");
    res.trace.push_back(query);
    json_prolog::PrologQueryProxy results = pl.query(query.c_str());

    char status = results.getStatus();
    if (status == 0) {
        res.success = false;
        res.trace.push_back(std::string("Test performance entry insertion into ontology FAILED, either invalid test or patient alias"));
        res.error = std::string("Test performance entry insertion into ontology FAILED, either invalid test or patient alias");
        return res;
    } else if (status == 3) {
        res.success = true;
    }
    std::vector<std::string> query_ret_tests;
    for (json_prolog::PrologQueryProxy::iterator it = results.begin();
            it != results.end(); it++) {
        json_prolog::PrologBindings bdg = *it;
        std::string temp_query_tests = bdg["B"];
        query_ret_tests.push_back(temp_query_tests);
    }
    for (unsigned int i = 0; i < query_ret_tests.size(); i++) {
        res.cognitive_test_performance_entry = (query_ret_tests[i]);
    }
    KnowrobWrapper::dump_ontology_now();
    return res;
}

/** 
 * @brief Implements the create_cognitve_tests ROS service 
 * @param req [rapp_platform_ros_communications::createCognitiveExerciseTestSrv::Request&] The ROS service request 
 * @return res [rapp_platform_ros_communications::createCognitiveExerciseTestSrv::Response&] The ROS service response 
 */
rapp_platform_ros_communications::createCognitiveExerciseTestSrv::Response KnowrobWrapper::create_cognitve_tests(rapp_platform_ros_communications::createCognitiveExerciseTestSrv::Request req) {
    rapp_platform_ros_communications::createCognitiveExerciseTestSrv::Response res;
    if (req.test_type == std::string("") || req.test_difficulty < 1 || req.test_path == std::string("") || req.test_subtype == std::string("")) {
        res.success = false;
        res.trace.push_back("Error, one or more arguments not provided or out of range. Test variation and test difficulty are positive integers >0");
        res.error = std::string("Error, one or more arguments not provided or out of range.  Test variation and test difficulty are positive integers >0");
        return res;
    }

    std::string path = ros::package::getPath("rapp_cognitive_exercise");
    std::string temp_check_path = path + req.test_path;
    const char * c = temp_check_path.c_str();
    if (!checkIfFileExists(c)) {
        res.success = false;
        res.trace.push_back(std::string("Test file does not exist in provided file path"));
        res.trace.push_back(req.test_path);
        res.error = std::string("Test file does not exist in provided file path");
        return res;
    }
    std::string difficulty = intToString(req.test_difficulty);
    std::string query = std::string("createCognitiveTest(knowrob:'") + req.test_type + std::string("',B,'") + difficulty + std::string("','") + req.test_path + std::string("',knowrob:'") + req.test_subtype + std::string("')");
    json_prolog::PrologQueryProxy results = pl.query(query.c_str());
    char status = results.getStatus();
    if (status == 0) {
        res.success = false;
        res.trace.push_back(std::string("Test insertion into ontology FAILED, possible error is test type/subtype invalid"));
        res.error = std::string("Test insertion into ontology FAILED, possible error is test type/subtype invalid");
        return res;
    } else if (status == 3) {
        res.success = true;
    }
    std::vector<std::string> query_ret_tests;
    for (json_prolog::PrologQueryProxy::iterator it = results.begin();
            it != results.end(); it++) {
        json_prolog::PrologBindings bdg = *it;
        std::string temp_query_tests = bdg["B"];
        query_ret_tests.push_back(temp_query_tests);
    }

    for (unsigned int i = 0; i < query_ret_tests.size(); i++) {
        res.test_name = (query_ret_tests[i]);
    }
    std::string tmp_test_name;
    tmp_test_name.assign(res.test_name.c_str());
    std::vector<std::string> test_created = split(tmp_test_name, std::string("#"));

    if (test_created.size() == 2) {
        for (unsigned int i = 0; i < req.supported_languages.size(); i++) {
            query = std::string("rdf_assert(knowrob:'") + test_created[1] + std::string("',knowrob:supportedLanguages,knowrob:'") + req.supported_languages[i] + std::string("')");
            results = pl.query(query.c_str());
        }
    }
    KnowrobWrapper::dump_ontology_now();
    return res;
}

/** 
 * @brief Implements the cognitive_tests_of_type ROS service 
 * @param req [rapp_platform_ros_communications::cognitiveTestsOfTypeSrv::Request&] The ROS service request 
 * @return res [rapp_platform_ros_communications::cognitiveTestsOfTypeSrv::Response&] The ROS service response 
 */
rapp_platform_ros_communications::cognitiveTestsOfTypeSrv::Response KnowrobWrapper::cognitive_tests_of_type(rapp_platform_ros_communications::cognitiveTestsOfTypeSrv::Request req) {
    rapp_platform_ros_communications::cognitiveTestsOfTypeSrv::Response res;
    try {
        if (req.test_type == std::string("")) {
            throw std::string("Error, test_type empty");
        }
        if (req.test_language == std::string("")) {
            throw std::string("Error, language empty");
        }
        std::string query = std::string("cognitiveTestsOfType(knowrob:'") + req.test_type + std::string("',B,Path,Dif,Sub,knowrob:'") + req.test_language + std::string("')");
        json_prolog::PrologQueryProxy results = pl.query(query.c_str());
        char status = results.getStatus();
        if (status == 0) {
            throw std::string("No tests of given type exist");

        }
        res.success = true;
        std::vector<std::string> query_ret_tests;
        std::vector<std::string> query_ret_scores;
        std::vector<std::string> query_ret_difficulty;
        std::vector<std::string> query_ret_file_paths;
        std::vector<std::string> query_ret_subtypes;
        for (json_prolog::PrologQueryProxy::iterator it = results.begin();
                it != results.end(); it++) {
            json_prolog::PrologBindings bdg = *it;
            std::string temp_query_tests = bdg["B"];
            query_ret_tests.push_back(temp_query_tests);
            std::string temp_query_file_paths = bdg["Path"];
            query_ret_file_paths.push_back(temp_query_file_paths);
            std::string temp_query_difficulty = bdg["Dif"];
            query_ret_difficulty.push_back(temp_query_difficulty);
            std::string temp_query_subtypes = bdg["Sub"];
            query_ret_subtypes.push_back(temp_query_subtypes);
        }
        for (unsigned int i = 0; i < query_ret_tests.size(); i++) {
            res.tests.push_back(query_ret_tests[i]);
            res.difficulty.push_back(query_ret_difficulty[i]);
            res.file_paths.push_back(query_ret_file_paths[i]);
            res.subtype.push_back(query_ret_subtypes[i]);
        }
        return res;
    } catch (std::string error) {
        res.success = false;
        res.trace.push_back(error);
        res.error = error;
        return res;
    }
}

/** 
 * @brief Implements the user_performance_cognitve_tests ROS service 
 * @param req [rapp_platform_ros_communications::userPerformanceCognitveTestsSrv::Request&] The ROS service request 
 * @return res [rapp_platform_ros_communications::userPerformanceCognitveTestsSrv::Response&] The ROS service response 
 */
rapp_platform_ros_communications::userPerformanceCognitveTestsSrv::Response KnowrobWrapper::user_performance_cognitve_tests(rapp_platform_ros_communications::userPerformanceCognitveTestsSrv::Request req) {
    rapp_platform_ros_communications::userPerformanceCognitveTestsSrv::Response res;
    try {
        if (req.ontology_alias == std::string("")) {
            throw std::string("Error, ontology alias empty");
        }
        if (req.test_type == std::string("")) {
            throw std::string("Error, test type empty");
        }
        std::string query = std::string("userCognitiveTestPerformance(knowrob:'") + req.ontology_alias + std::string("',knowrob:'") + req.test_type + std::string("',B,Dif,Timestamp,SC,P,SubType)");
        json_prolog::PrologQueryProxy results = pl.query(query.c_str());
        char status = results.getStatus();
        if (status == 0) {
            throw std::string("No performance records exist for the user or invalid user or invalid test type");
        }
        res.success = true;
        std::vector<std::string> query_ret_tests;
        std::vector<std::string> query_ret_scores;
        std::vector<std::string> query_ret_difficulty;
        std::vector<std::string> query_ret_timestamps;
        std::vector<std::string> query_ret_subtypes;
        for (json_prolog::PrologQueryProxy::iterator it = results.begin();
                it != results.end(); it++) {
            json_prolog::PrologBindings bdg = *it;
            std::string temp_query_tests = bdg["B"];
            query_ret_tests.push_back(temp_query_tests);
            std::string temp_query_difficulty = bdg["Dif"];
            query_ret_difficulty.push_back(temp_query_difficulty);
            std::string temp_query_timestamps = bdg["Timestamp"];
            query_ret_timestamps.push_back(temp_query_timestamps);
            std::string temp_query_scores = bdg["SC"];
            query_ret_scores.push_back(temp_query_scores);
            std::string tmp_query_subtypes = bdg["SubType"];
            query_ret_subtypes.push_back(tmp_query_subtypes);
        }
        for (unsigned int i = 0; i < query_ret_tests.size(); i++) {
            res.tests.push_back(query_ret_tests[i]);
            res.scores.push_back(query_ret_scores[i]);
            res.difficulty.push_back(query_ret_difficulty[i]);
            res.timestamps.push_back(query_ret_timestamps[i]);
            res.subtypes.push_back(query_ret_subtypes[i]);
        }
        return res;
    } catch (std::string error) {
        res.success = false;
        res.trace.push_back(error);
        res.error = error;
        return res;
    }
}

/** 
 * @brief Implements the clear_user_cognitive_tests_performance_records ROS service 
 * @param req [rapp_platform_ros_communications::clearUserPerformanceCognitveTestsSrv::Request&] The ROS service request 
 * @return res [rapp_platform_ros_communications::clearUserPerformanceCognitveTestsSrv::Response&] The ROS service response 
 */
rapp_platform_ros_communications::clearUserPerformanceCognitveTestsSrv::Response KnowrobWrapper::clear_user_cognitive_tests_performance_records(rapp_platform_ros_communications::clearUserPerformanceCognitveTestsSrv::Request req) {
    rapp_platform_ros_communications::clearUserPerformanceCognitveTestsSrv::Response res;
    if (req.username == std::string("")) {
        res.success = false;
        res.trace.push_back("Error, ontology alias empty");
        res.error = std::string("Error, ontology alias empty");
        return res;
    }
    std::string currentAlias = get_ontology_alias(req.username);
    if (req.test_type == std::string("")) {
        std::string query = std::string("rdf_has(P,knowrob:cognitiveTestPerformedPatient,knowrob:'") + currentAlias + std::string("'),rdf_retractall(P,L,S)");
        json_prolog::PrologQueryProxy results = pl.query(query.c_str());
        char status = results.getStatus();
        if (status == 0) {
            res.success = false;
            res.error = std::string("No performance records exist for the user or invalid user or invalid test type");
            return res;
        } else if (status == 3) {
            res.success = true;
        }
    } else {
        std::string query = std::string("rdf_has(A,rdf:type,knowrob:'") + req.test_type + std::string("'),rdf_has(P,knowrob:cognitiveTestPerformedTestName,A),rdf_has(P,knowrob:cognitiveTestPerformedPatient,knowrob:'") + currentAlias + std::string("'),rdf_retractall(P,L,S)");
        json_prolog::PrologQueryProxy results = pl.query(query.c_str());
        char status = results.getStatus();
        if (status == 0) {
            res.success = false;
            res.error = std::string("No performance records exist for the user or invalid user or invalid test type");
            return res;
        } else if (status == 3) {
            res.success = true;
        }
    }
    KnowrobWrapper::dump_ontology_now();
    return res;
}

/** 
 * @brief Implements the create_ontology_alias ROS service 
 * @param req [rapp_platform_ros_communications::createOntologyAliasSrv::Request&] The ROS service request 
 * @return res [rapp_platform_ros_communications::createOntologyAliasSrv::Response&] The ROS service response 
 */
rapp_platform_ros_communications::createOntologyAliasSrv::Response KnowrobWrapper::create_ontology_alias(rapp_platform_ros_communications::createOntologyAliasSrv::Request req) {
    rapp_platform_ros_communications::createOntologyAliasSrv::Response res;
    try {
        if (req.username == std::string("")) {
            throw std::string("Error, empty username");
        }
        std::string currentAlias = get_ontology_alias(req.username);
        std::size_t found = currentAlias.find(std::string("FAIL"));
        if (found != std::string::npos) {
            throw currentAlias;
        }
        res.ontology_alias = currentAlias;
        res.success = true;
        KnowrobWrapper::dump_ontology_now();
        return res;
    } catch (std::string error) {
        res.success = false;
        res.trace.push_back(error);
        res.error = error;
        return res;
    }
}

/** 
 * @brief Implements the subclassesOf ROS service 
 * @param req [rapp_platform_ros_communications::ontologySubSuperClassesOfSrv::Request&] The ROS service request 
 * @return res [rapp_platform_ros_communications::ontologySubSuperClassesOfSrv::Response&] The ROS service response 
 */
rapp_platform_ros_communications::ontologySubSuperClassesOfSrv::Response KnowrobWrapper::subclassesOfQuery(rapp_platform_ros_communications::ontologySubSuperClassesOfSrv::Request req) {
    rapp_platform_ros_communications::ontologySubSuperClassesOfSrv::Response res;
    try {
        if (req.ontology_class == std::string("")) {
            throw std::string("Error, empty ontology class");
        }
        std::string query = std::string("");
        if (req.recursive == true) {
            query = std::string("superclassesOf_withCheck(knowrob:'") + req.ontology_class + std::string("',A)");
        } else {
            query = std::string("direct_superclassesOf_withCheck(knowrob:'") + req.ontology_class + std::string("',A)");
        }

        json_prolog::PrologQueryProxy results = pl.query(query.c_str());
        char status = results.getStatus();
        if (status == 0) {
            throw std::string(std::string("Class: ") + req.ontology_class + std::string(" does not exist"));
        }

        res.success = true;
        std::vector<std::string> query_ret;
        for (json_prolog::PrologQueryProxy::iterator it = results.begin();
                it != results.end(); it++) {
            json_prolog::PrologBindings bdg = *it;
            //start remove duplicates
            int i;
            int logic = 0;
            std::string temp_query_result = bdg["A"];
            std::size_t found = temp_query_result.find(std::string("file:///"));
            if (found == std::string::npos) {
                for (int i = 0; i < query_ret.size(); i++) {
                    if (query_ret.at(i) == temp_query_result) {
                        logic = 1;
                    }
                }
                if (logic == 0) {
                    query_ret.push_back(temp_query_result);
                }
                //end removing duplicates
            }
        }
        for (unsigned int i = 0; i < query_ret.size(); i++) {
            res.results.push_back(query_ret[i]);
        }
        return res;

    } catch (std::string error) {
        res.success = false;
        res.trace.push_back(error);
        res.error = error;
        return res;
    }
}

/** 
 * @brief Implements the superclassesOf ROS service 
 * @param req [rapp_platform_ros_communications::ontologySubSuperClassesOfSrv::Request&] The ROS service request 
 * @return res [rapp_platform_ros_communications::ontologySubSuperClassesOfSrv::Response&] The ROS service response 
 */
rapp_platform_ros_communications::ontologySubSuperClassesOfSrv::Response KnowrobWrapper::superclassesOfQuery(rapp_platform_ros_communications::ontologySubSuperClassesOfSrv::Request req) {
    rapp_platform_ros_communications::ontologySubSuperClassesOfSrv::Response res;
    try {
        if (req.ontology_class == std::string("")) {
            throw std::string("Error, empty ontology class");
        }
        std::string query = std::string("");
        if (req.recursive == true) {
            query = std::string("subclassesOf_withCheck(knowrob:'") + req.ontology_class + std::string("',A)");
        } else {
            query = std::string("direct_subclassesOf_withCheck(knowrob:'") + req.ontology_class + std::string("',A)");
        }
        json_prolog::PrologQueryProxy results = pl.query(query.c_str());
        char status = results.getStatus();
        if (status == 0) {
            throw std::string(std::string("Class: ") + req.ontology_class + std::string(" does not exist"));
        }
        res.success = true;
        std::vector<std::string> query_ret;
        for (json_prolog::PrologQueryProxy::iterator it = results.begin();
                it != results.end(); it++) {
            json_prolog::PrologBindings bdg = *it;
            //start remove duplicates
            int i;
            int logic = 0;
            std::string temp_query_result = bdg["A"];
            std::size_t found = temp_query_result.find(std::string("file:///"));
            if (found == std::string::npos) {
                for (int i = 0; i < query_ret.size(); i++) {
                    if (query_ret.at(i) == temp_query_result) {
                        logic = 1;
                    }
                }
                if (logic == 0) {
                    query_ret.push_back(temp_query_result);
                }
                //end removing duplicates
            }
        }
        for (unsigned int i = 0; i < query_ret.size(); i++) {
            res.results.push_back(query_ret[i]);
        }
        return res;
    } catch (std::string error) {
        res.success = false;
        res.trace.push_back(error);
        res.error = error;
        return res;
    }
}

/** 
 * @brief Implements the isSubSuperclass ROS service 
 * @param req [rapp_platform_ros_communications::ontologyIsSubSuperClassOfSrv::Request&] The ROS service request 
 * @return res [rapp_platform_ros_communications::ontologyIsSubSuperClassOfSrv::Response&] The ROS service response 
 */
rapp_platform_ros_communications::ontologyIsSubSuperClassOfSrv::Response KnowrobWrapper::isSubSuperclassOfQuery(rapp_platform_ros_communications::ontologyIsSubSuperClassOfSrv::Request req) {
    rapp_platform_ros_communications::ontologyIsSubSuperClassOfSrv::Response res;
    try {
        if (req.parent_class == std::string("")) {
            throw std::string("Error, empty ontology class");
        }
        if (req.child_class == std::string("")) {
            throw std::string("Error, empty other class");
        }
        std::string query = std::string("");
        if (req.recursive == true) {
            query = std::string("superclassesOf_withCheck(knowrob:'") + req.parent_class + std::string("',A)");
        } else {
            query = std::string("direct_superclassesOf_withCheck(knowrob:'") + req.parent_class + std::string("',A)");
        }
        json_prolog::PrologQueryProxy results = pl.query(query.c_str());
        char status = results.getStatus();
        if (status == 0) {
            throw std::string(std::string("Class: ") + req.parent_class + std::string(" does not exist"));
        }
        res.success = true;
        std::vector<std::string> query_ret;
        int logic = 0;
        for (json_prolog::PrologQueryProxy::iterator it = results.begin();
                it != results.end(); it++) {
            json_prolog::PrologBindings bdg = *it;
            std::string temp_query_result = bdg["A"];
            std::vector<std::string> seperator = split(temp_query_result, std::string("#"));
            if (seperator[1] == req.child_class) {
                logic = 1;
                break;
            }
        }
        if (logic == 0) {
            res.result = false;
        } else {
            res.result = true;
        }
        return res;
    } catch (std::string error) {
        res.success = false;
        res.trace.push_back(error);
        res.error = error;
        return res;
    }
}

/** 
 * @brief Implements the createInstance ROS service 
 * @param req [rapp_platform_ros_communications::createInstanceSrv::Request&] The ROS service request 
 * @return res [rapp_platform_ros_communications::createInstanceSrv::Response&] The ROS service response 
 */
rapp_platform_ros_communications::createInstanceSrv::Response KnowrobWrapper::createInstanceQuery(rapp_platform_ros_communications::createInstanceSrv::Request req) {
    rapp_platform_ros_communications::createInstanceSrv::Response res;
    try {
        if (req.username == std::string("")) {
            throw std::string("Error, empty username");
        }
        if (req.ontology_class == std::string("")) {
            throw std::string("Error, empty ontology class");
        }
        std::string ontology_alias = get_ontology_alias(req.username);
        //ontology_alias
        std::size_t found = ontology_alias.find(std::string("FAIL"));
        if (found != std::string::npos) {
            throw ontology_alias;
        }
        std::vector<std::string> instance_name;
        std::string query = std::string("instanceFromClass_withCheck_andAssign(knowrob:'") + req.ontology_class + std::string("',A,knowrob:'") + ontology_alias + std::string("')");
        res.trace.push_back(query);
        json_prolog::PrologQueryProxy results = pl.query(query.c_str());
        char status = results.getStatus();
        if (status == 0) {
            throw std::string(std::string("Class: ") + req.ontology_class + std::string(" does not exist probably.. or ontology_alias for user exists in the mysqlDatabase and not in the ontology"));
        }
        res.success = true;
        for (json_prolog::PrologQueryProxy::iterator it = results.begin(); it != results.end(); it++) {
            json_prolog::PrologBindings bdg = *it;
            instance_name.push_back(bdg["A"]);
        }
        if (instance_name.size() == 1) {
            instance_name = split(instance_name[0], "#");
            if (instance_name.size() == 2) {
                res.instance_name = (std::string("Created instance name is: ") + instance_name[1]);
            } else {
                throw std::string("Fatal Error, instance not created.. split to # error");
            }
        } else {
            throw std::string("Fatal Error, instance not created.., retrieval error");
        }
        KnowrobWrapper::dump_ontology_now();
        return res;
    } catch (std::string error) {
        res.success = false;
        res.trace.push_back(error);
        res.error = error;
        return res;
    }
}

/** 
 * @brief Implements the dumpOntology ROS service 
 * @param req [rapp_platform_ros_communications::ontologyLoadDumpSrv::Request&] The ROS service request 
 * @return res [rapp_platform_ros_communications::ontologyLoadDumpSrv::Response&] The ROS service response 
 */
rapp_platform_ros_communications::ontologyLoadDumpSrv::Response KnowrobWrapper::dumpOntologyQuery(rapp_platform_ros_communications::ontologyLoadDumpSrv::Request req) {
    rapp_platform_ros_communications::ontologyLoadDumpSrv::Response res;
    try {
        std::string path = getenv("HOME");
        path = path + std::string("/rapp_platform_files/");
        if (req.file_url.empty()) { // || req.file_url==std::string("")        
            throw std::string("Empty file path");
        }
        size_t pathDepth = std::count(req.file_url.begin(), req.file_url.end(), '/');
        std::string str2("/");
        std::size_t found = req.file_url.find(str2);
        if (found != std::string::npos && pathDepth > 1) {
            std::string folderFromPath = SplitFilename(req.file_url);
            const char * c = folderFromPath.c_str();
            if (!checkIfFileExists(c)) {
                throw std::string("Path does not exist, invalid folder?");
            }
        }
        req.file_url = path + req.file_url;
        std::string query = std::string("rdf_save('") + req.file_url + std::string("')");
        json_prolog::PrologQueryProxy results = pl.query(query.c_str());
        char status = results.getStatus();
        if (status == 0) {
            throw std::string("Ontology dump failed");
        }
        res.success = true;
        return res;
    } catch (std::string error) {
        res.success = false;
        res.trace.push_back(error);
        res.error = error;
        return res;
    }
}

/** 
 * @brief Implements the loadOntology ROS service 
 * @param req [rapp_platform_ros_communications::ontologyLoadDumpSrv::Request&] The ROS service request 
 * @return res [rapp_platform_ros_communications::ontologyLoadDumpSrv::Response&] The ROS service response 
 */
rapp_platform_ros_communications::ontologyLoadDumpSrv::Response KnowrobWrapper::loadOntologyQuery(rapp_platform_ros_communications::ontologyLoadDumpSrv::Request req) {
    rapp_platform_ros_communications::ontologyLoadDumpSrv::Response res;
    try {
        if (req.file_url.empty()) {
            throw std::string("Empty file path");
        }
        std::string path = getenv("HOME");
        path = path + std::string("/rapp_platform_files/");
        req.file_url = path + req.file_url;
        const char * c = req.file_url.c_str();
        if (!checkIfFileExists(c)) {
            throw std::string(std::string("File does not exist in provided file path: '")
                    + std::string(req.file_url) + std::string("'"));
        }
        std::string query = std::string("rdf_load('") + req.file_url + std::string("')");
        json_prolog::PrologQueryProxy results = pl.query(query.c_str());
        char status = results.getStatus();
        if (status == 0) {
            throw std::string("Ontology load failed");
        }
        res.success = true;
        return res;
    } catch (std::string error) {
        res.success = false;
        res.trace.push_back(error);
        res.error = error;
        return res;
    }
}

/** 
 * @brief Implements the returnUserInstancesOfClass ROS service 
 * @param req [rapp_platform_ros_communications::returnUserInstancesOfClassSrv::Request&] The ROS service request 
 * @return res [rapp_platform_ros_communications::returnUserInstancesOfClassSrv::Response&] The ROS service response 
 */
rapp_platform_ros_communications::returnUserInstancesOfClassSrv::Response KnowrobWrapper::user_instances_of_class(rapp_platform_ros_communications::returnUserInstancesOfClassSrv::Request req) {
    rapp_platform_ros_communications::returnUserInstancesOfClassSrv::Response res;
    try {
        if (req.username == std::string("")) {
            throw std::string("Error, empty username");
        }
        if (req.ontology_class == std::string("")) {
            throw std::string("Error, empty ontology class");
        }
        std::string ontology_alias = get_ontology_alias(req.username);
        std::string query = std::string("rdf_has(knowrob:'") + ontology_alias + std::string("',knowrob:'belongsToUser',A)");
        json_prolog::PrologQueryProxy results = pl.query(query.c_str());
        char status = results.getStatus();
        if (status == 0) {
            throw std::string("User has no instances");
        }
        res.success = true;
        std::vector<std::string> query_ret;
        for (json_prolog::PrologQueryProxy::iterator it = results.begin();
                it != results.end(); it++) {
            json_prolog::PrologBindings bdg = *it;
            //start removing duplicates
            int i;
            int logic = 0;
            std::string temp_query_result = bdg["A"];
            if (req.ontology_class != std::string("*")) {
                std::size_t found = temp_query_result.find(std::string("#") + req.ontology_class + std::string("_"));
                if (found != std::string::npos) {
                    query_ret.push_back(temp_query_result);
                }
            } else {
                query_ret.push_back(temp_query_result);
            }
            //end removing duplicates
        }
        for (unsigned int i = 0; i < query_ret.size(); i++) {
            res.results.push_back(query_ret[i]);
        }
        return res;
    } catch (std::string error) {
        res.success = false;
        res.trace.push_back(error);
        res.error = error;
        return res;
    }
}


