hop = require('hop');
//Fs = require('./fileUtils.js');
fs = require('fs');

console.log(hop.hostname);
console.log(hop.port);

var user = process.env.LOGNAME;
var storePath = "/home/" + user + "/hop_temps/"; 
var rosService = "/ric/speech_detection_sphinx4_batch";
var randStringGen = require('../utilities/./randStringGen.js');

/*----<Random String Generator configurations---->*/
var stringLength = 5;
var randStrGen = new randStringGen( stringLength );
/*------------------------------------------------*/


service speech_detection_sphinx4( {fileUrl: '', language: '', audio_source: '', words: [], sentences: [], grammar: [], user: ''} ){
  console.log('Service invocation. Preparing response');
  console.log('Audio source file stored at:', fileUrl);
  console.log('Words to search for:', words);
  console.log('Sentences:', sentences);
  console.log('Grammar:', grammar);

  /* --< Perform renaming on the reived file. Add uniqueId value> --- */
  var unqExt = randStrGen.createUnique();
  randStrGen.removeCached(unqExt);
  var file = fileUrl.split('.');
  var fileUri_new = file[0] + '.' + file[1] +  unqExt + '.' + file[2]
  fs.renameSync(fileUrl, fileUri_new);
  /*----------------------------------------------------------------- */

  return hop.HTTPResponseAsync(
    function( sendResponse ) { 
      // sendResponse is a function argument that is generated by Hop to complete the service when the response is ready
      // here you need to send a message to rosbridge, prepare a callback to handle the result, and then  sendResponse( result ) in the callback body to actually return the service result.
      //var fileName = "speech-" + "bitch" + ".wav";
      //var audioFileUrl = Fs.resolvePath( storePath + fileName );
      //Fs.writeFileSync( audioFileUrl, fileData );
      console.log(typeof words)
      var args = {
        'path': /*audioFileUrl*/fileUri_new,
        'audio_source': audio_source,
        'words': JSON.parse(words),
        'sentences': JSON.parse(sentences),
        'grammar': JSON.parse(grammar),
        'language': language,
        'user': user
      };

      var uniqueID = randStrGen.createUnique();
      var ros_srv_call = {
        'op': 'call_service',
         'service': rosService,
         'args': args,
         'id': uniqueID
      };

      var rosWS = new WebSocket('ws://localhost:9090');
      rosWS.onopen = function(){
        console.log('Connection to rosbridge established');
        this.send(JSON.stringify(ros_srv_call));
      }
      rosWS.onclose = function(){
        console.log('Connection to rosbridge closed');
      }
      rosWS.onmessage = function(event){
        console.log('Received message from rosbridge');
        var resp_msg = event.value;
        sendResponse( resp_msg );
        console.log(resp_msg);
        this.close();
        rosWS = undefined;
        randStrGen.removeCached( uniqueID );
      }
      
      setTimeout( function(){
        if (rosWS != undefined ){
          console.log('Connection timed out!');
          sendResponse('false');
          rosWS.close();
        }
      }, 10000);

    }, this ); // do not forget the <this> argument of hop.HTTResponseAsync 
}

//function craft_ros_srv_args(file_url)
//{
  //var timeNow = new Date().getTime();
  //var args = {};

  //args['path'] = 
  //return args;
//};
