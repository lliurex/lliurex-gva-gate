#!/usr/bin/python3

from flask import Flask, json, request

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

if __name__ == '__main__':
    api.run()
