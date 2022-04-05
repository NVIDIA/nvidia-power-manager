## Nvidia power management app ##

### Power Manager configurations ###
The power mangment configuration file is devided into four objects 
> **PowerRedundancyConfigs** ,**powerState**, **powerCappingConfigs** and **powerCappingSavePath**

#### PowerRedundancyConfigs ####
This object consist of keys used to configure the Power redundancy functionality of the Power managment app below are the definitions of the Used.

**objectName -** this key will pass the object name of the property to be monitored.
> **ex:** "objectName": "/xyz/openbmc_project/sensors/power/psu_drop_to_1_event"

**interfaceName -** this key will pass the interface name of the property to be monitored.
> **ex:** "interfaceName": "xyz.openbmc_project.Object.Enable"

**propertyName -** this key will pass property to be monitored.
> **ex:** "propertyName": "PowerCap"

**action -** this key contains array of actions need to be performed when propertyName is triggered. 
> **ex:**
> 
>             "action": [
>                 {
>                     "trigger": true,
>                     "conditionBlock": [
>                         {
>                             "serviceName": "com.Nvidia.PsuEvent",
>                             "objectpath": "/xyz/openbmc_project/sensors/power/MBON_GBOFF_event",
>                             "interfaceName": "xyz.openbmc_project.Object.Enable" ,
>                             "propertyName": "Enabled",
>                             "timeDelay": 10,
>                             "propertyValue": true
> 
>                         }
>                     ],
>                     "methodName": "Set",
>                     "serviceName": "xyz.openbmc_project.State.Chassis",
>                     "objectpath": "/xyz/openbmc_project/state/chassis0",
>                     "interfaceName": "org.freedesktop.DBus.Properties",
>                     "appendData": [
>                         {
>                             "dataType": "s",
>                             "data": "xyz.openbmc_project.State.Chassis"
>                         },
>                         {
>                             "dataType": "s",
>                             "data": "RequestedPowerTransition"
>                         },
>                         {
>                             "dataType": "s",
>                             "data": "xyz.openbmc_project.State.Chassis.Transition.On" ,
>                             "variant": true
>                         }
>                     ]
>                 }
>             ]

**trigger -** this key in action object gives the state on which the action block should be executed.
> **ex:**"trigger": true

**methodName -** this key in action object gives the method to be called when trigger check is true.
> **ex:**"methodName": "Set"

**serviceName -** this key in action object gives the serviceName to be passed to the method call.
> **ex:**"serviceName": "xyz.openbmc_project.State.Chassis"

**objectpath -** this key in action object gives the objectpath to be passed to the method call.
> **ex:**"objectpath": "/xyz/openbmc_project/state/chassis0"

**interfaceName -** this key in action object gives the interfaceName to be passed to the method call.
> **ex:**"interfaceName": "org.freedesktop.DBus.Properties"

**appendData -** this object containsarray of the input data for the method call.
> **ex:**
> 
>                     "appendData": [
>                         {
>                             "dataType": "s",
>                             "data": "xyz.openbmc_project.State.Chassis"
>                         },
>                         {
>                             "dataType": "s",
>                             "data": "RequestedPowerTransition"
>                         },
>                         {
>                             "dataType": "s",
>                             "data": "xyz.openbmc_project.State.Chassis.Transition.On" ,
>                             "variant": true
>                         }
>                     ]

**dataType -** this key in appendData object contains the dbus data type of input data for the method call
> **ex:**"dataType": "s"

**data -** this key in appendData object contains the input data for the method call
> **ex:**"data": "RequestedPowerTransition"

**key -** this key should be added if data is of OEM type. when input data is "OEM" this indicates the input data is OEM specific and it should be handled in **oemKeyHandler** function depending on the "key" value.
> **ex:**
> 
>                                 {
>                                     "data": "OEM",
>                                     "key": "AddSel"
>                                 }


**variant -** this key  if present in appendData object the input data is of type variant type for the method call
> **ex:**"variant": true


**conditionBlock -** this key provides the condition block which needs to be validated before the action block is executed. this block is optional if present the condition is checked, if not the action is performed by default.
> **ex:**
> 
>      "conditionBlock": [
>     {
>     "serviceName": "com.Nvidia.PsuEvent",
>     "objectpath": "/xyz/openbmc_project/sensors/power/MBON_GBOFF_event",
>     "interfaceName": "xyz.openbmc_project.Object.Enable" ,
>     "propertyName": "Enabled",
>     "timeDelay": 10,
>     "propertyValue": true
>     
>     }
>     ]

**serviceName -** the service which provides the property which is used in validation.
> **ex:** "serviceName": "com.Nvidia.PsuEvent"

**objectpath -** the objectpath which provides the property which is used in validation.
> **ex:** "objectpath": "/xyz/openbmc_project/sensors/power/MBON_GBOFF_event"

**interfaceName -** the interfaceName which provides the property which is used in validation.
> **ex:** "interfaceName": "xyz.openbmc_project.Object.Enable"

**propertyName -** the propertyName which is used in validation.
> **ex:** "propertyName": "Enabled"

**timeDelay -** the time delay before executing the conditional block.
> **ex:** "timeDelay": 10

**propertyValue -** the propertyValue which should be matched for the condition to be true.
> **ex:** "propertyValue": true


#### powerState ####
This object consist of keys used to configure the functionality of the Power managment app when power is ON/OFF.all the keys used are similar to **PowerRedundancyConfigs**.
> **ex:**
> 
>     "powerState": {
>         "objectName": "/xyz/openbmc_project/state/chassis0",
>         "interfaceName": "xyz.openbmc_project.State.Chassis",
>         "propertyName": "CurrentPowerState",
>         "action": [
>             {
>                 "trigger": "xyz.openbmc_project.State.Chassis.PowerState.On",
>                 "methodName": "IpmiSelAddOem",
>                 "serviceName": "xyz.openbmc_project.Logging.IPMI",
>                 "objectpath": "/xyz/openbmc_project/Logging/IPMI",
>                 "interfaceName": "xyz.openbmc_project.Logging.IPMI",
>                 "appendData": [
>                     {
>                         "dataType": "s",
>                         "data": "chassis power on  SEL Entry"
>                     },
>                     {
>                         "dataType": "y",
>                         "data": [0, 255 , 255]
>                     },
>                     {
>                         "dataType": "y",
>                         "data": 201
>                     }
>                 ]
>             }
>         ]
>     }

#### powerCappingConfigs ####
This object consist of keys used to configure the Power capping functionality of the Power managment app. it lists the the properties to be populated as part of the power manager service.

**objectName -** the obect path of the property to be populated.
> **ex:**"objectName": "/xyz/openbmc_project/control/power/CurrentChassisLimit"

**interfaceName -** the interface on which the property is populated.
> **ex:**"objectName": "/xyz/openbmc_project/control/power/CurrentChassisLimit"

**property -** this block gives the properties and the action to be triggered when this property is updated.
> **ex:**
> 
>             "property": [
>                 {
>                     "propertyName": "PowerCap",
>                      "value": 6500,
>                      "flags": "sdbusplus::asio::PropertyPermission::readWrite" ,
>                      "action":[
>                         {
>                             "methodName": "IpmiSelAddOem",
>                             "serviceName": "xyz.openbmc_project.Logging.IPMI",
>                             "objectpath": "/xyz/openbmc_project/Logging/IPMI",
>                             "interfaceName": "xyz.openbmc_project.Logging.IPMI",
>                             "appendData": [
>                                 {
>                                     "dataType": "s",
>                                     "data": "OEM current chassis limit changed SEL Entry"
>                                 },
>                                 {
>                                     "data": "OEM",
>                                     "key": "AddSel"
>                                 },
>                                 {
>                                     "dataType": "y",
>                                     "data": 201
>                                 }
>                             ]
>                         }
>                      ]    
>                 },
>                 {
>                     "propertyName": "MinPowerCapValue",
>                      "value": 4900,
>                      "flags": "sdbusplus::asio::PropertyPermission::readOnly"
>                 },
>                 {
>                     "propertyName": "MaxPowerCapValue",
>                      "value": 6500,
>                      "flags": "sdbusplus::asio::PropertyPermission::readOnly"
>                 }
>             ]

**propertyName -** the Name of the property to be populated.
> **ex:**"propertyName": "PowerCap"

**value -** the default value of the property to be populated.
> **ex:**"value": 6500

**flags -** the flags of the property to be populated.
> **ex:**"flags": "sdbusplus::asio::PropertyPermission::readWrite"

**Note -** the action block is similar to **PowerRedundancyConfigs**

#### powerCappingAlgorithm ####
This section of the json file is used to configure the percentage of the power consumed by each module.
**powerModule -**: the Name of the Module 
> **ex:** "powerModule":"GPU"

**powerCapPercentage -**: the percentage of the total power consumed by the Module 
> **ex:** "powerCapPercentage":49

**numOfDevices -**: The total number of devices present in the module .
> **ex:** "numOfDevices": 8

#### powerCappingSavePath ####

this key provides the PATH where the power capping property dump is stored .
> **ex:**"powerCappingSavePath":"/etc/powerCap.bin" 
