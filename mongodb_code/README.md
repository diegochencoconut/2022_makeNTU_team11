# Database connection guide

## Setup

Use following code to setup:
    import requests
    import json

    preurl = "http://*ip_address:port*/"
    # example: "http://170.1.1.1:3000/"
    # Don't forget to add slash at the end!
    # Remember to connect to same wifi as database!
    # default port: 3000
    headers = {'Content-type': 'application/json', 'Accept': 'text/plain'}


## Part 1: Evaluation

url: preurl + "evaluation"
Data format:
`{ "room": "*room_number*", "symptom": "*true/false*", "temperature": "*a number*" }`

code example:
    url = preurl + "evaluation"
    data = {'room': '335', 'symptom': 'false', 'temperature': '37.5'}  #remember to adjust the data
    r = requests.post(url, data=json.dumps(data), headers=headers)
    print(r.text)

If everything work successfully, you will receive `{"message": "success"}`

## Part 2: Add_meal

url: preurl + "add_meal"
Data format:
`{ "room": "*room_number*", "order": "*chicken/pork/vegan* }`

code example:
    url = preurl + "add_meal"
    data = {'room': '335', 'order': 'pork'}  #remember to adjust the data
    r = requests.post(url, data=json.dumps(data), headers=headers)
    print(r.text)

If everything work successfully, you will receive `{"message": "success"}`

## Part 3: Door control

### Case 1: Initializing
**This code must be executed in the beginning of the script**
**And should only be executed ONCE**
*The usage of this code is to check the room exist in database*

url: preurl + "door"
Data format:
`{ "room": "*room_number*", "door": "*request/open/close*"}`

code example:
    url = preurl + "door"
    data = {'room': '335', 'door": "close'}  #remember to adjust the data
    r = requests.post(url, data=json.dumps(data), headers=headers)
    print(r.text)

If everything work successfully, there will be two possible replies:
1. If the database already has this room, it would return `{"message": "Room existed"}`
2. If the database does not has this room, it would return `{"message": "The room is created"}`

### Case 2: Change door status (including open and close)
url: preurl + "door/*your room number*" 
**Attention: Don't forget to add room number**
Data format:
`{"door": "*open/close*"}`

code example:
    url = preurl + "door/*your room number*"
    data = {'door": "open'}  #remember to adjust the data
    r = requests.patch(url, data=json.dumps(data), headers=headers)
    print(r.text)

If everything work successfully, you will receive `{"message": "success"}`

### Case 3: Request to open door
url: preurl + "door/*your room number*"
Data format:
`{"door": "request"}`

code example:
    url = preurl + "door/*your room number*"
    data = {'door": "request'}
    r = requests.patch(url, data=json.dumps(data), headers=headers)
    print(r.text)

If everything work successfully, you will receive following messages:
1. If the user is authorized to open the door, you will receive `{"message": "accepted"}`
2. If the user request is rejected, you will receive `{"message": "rejected"}`

### Case 4: Check if anyone else open the door between request and really open the door
This code is using when the user made an accpeted request, the system will automatically check if anyone else open the door for a short time.

url: preurl + "door"
No data required.

code example
    url = preurl + "door"
    r = requests.get(url, headers = headers)
    print(r.text)

If everything work successfully, you will receive following messages:
1. If NO ONE ELSE open the door, you will receive `{"message": "accepted"}`
2. If SOMEONE open the door, you will receive `{"message": "rejected"}`

## How to get data from JSON by python?
We are going to use the library *json*, which is already installed with python.

On above examples, `r.text` is a json string. We need to use following code:

    #Assume that r.text is {"message": "accepted"}
    received = json.loads(r.text)
    message = received["message"]
    print(message)

The code above should print `accepted`
