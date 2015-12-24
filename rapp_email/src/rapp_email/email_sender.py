#!/usr/bin/env python
# -*- encode: utf-8 -*-

#Copyright 2015 RAPP

#Licensed under the Apache License, Version 2.0 (the "License");
#you may not use this file except in compliance with the License.
#You may obtain a copy of the License at

    #http://www.apache.org/licenses/LICENSE-2.0

#Unless required by applicable law or agreed to in writing, software
#distributed under the License is distributed on an "AS IS" BASIS,
#WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#See the License for the specific language governing permissions and
#limitations under the License.

# Authors: Aris Thallas
# contact: aris.thallas@{iti.gr, gmail.com}

import rospy

from rapp_platform_ros_communications.srv import (
  SendEmailSrv,
  SendEmailSrvResponse
  )

class EmailSender(object):

  def __init__(self):
    sendSrvTopic = rospy.get_param("rapp_email_send_topic")
    sendSrv = rospy.Service(sendSrvTopic, SendEmailSrv, \
                      self.sendEmailSrvCallback)

  ## The callback to send specified mails from users email account
  #
  # @rapam req [rapp_platform_ros_communications::Email::SendEmailSrvRequest] The send email request
  #
  # @rapam res [rapp_platform_ros_communications::Email::SendEmailSrvResponse] The send email response
  def sendEmailSrvCallback(self, req):
    pass