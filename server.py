#!/usr/bin/python3

from flask import Flask, json, request, Response

group = [{"gid": 10003, "name": "students", "members": ["jaume","alu01","alu02"]}, {"gid": 10004, "name": "teachers","members":["quique"]}]

passwd = {"alu01" : "alu01secret"}

api = Flask(__name__)

@api.route('/get_group', methods=['GET'])
def get_group():
  return json.dumps(group)

@api.route('/login', methods=['GET'])
def login():
  user = request.args.get("user")
  pw = passwd.get(user)
  success = pw!=None and pw==request.args.get("password")
  data = {"user":user,"success":success}
  if (success):
      data["group"] = group
  return json.dumps(data)

@api.route('/', methods=['POST'])
def autenticate():
    data = {"user": request.form.get("user"), "groups":[group]}
    try:
        status = passwd[request.form.get("user")] == request.form.get("passwd")
    except:
        status = False
    if status: 
        return json.dumps(data)
    else:
        return Response(response="Unauthorized",status=401) 

if __name__ == '__main__':
    api.run()
