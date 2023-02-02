#!/usr/bin/python3

from datetime import date
from flask import Flask, json, request, Response

group = [{"gid": 10003, "name": "students", "members": ["jaume","alu01","alu02"]}, {"gid": 10004, "name": "teachers","members":["quique"]}]

passwd = {"alu01" : "alu01secret"}

ldap_information = {"alu01":{"password":"alu01secret","gid":{"Domain Users":288400513}, "groups":{"teachers":10003,"GRP_03000394":288412920,"DenegarPermisosListadoAD":74373983,"Docente":288412920},"uid":288430185,"name":"Alumno","surname":"Estudiante","home":"/home/alu01"}}

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

@api.route('/authenticate', methods=['POST'])
def authenticate():
    login_name = request.form.get("user")
    try:
        status = passwd[login_name] == request.form.get("passwd")
        data = {"user":
                {
                    "login":login_name,
                    "uid":ldap_information[login_name]["uid"],
                    "gid":{"Domain Users": 288400513},
                    "name":ldap_information[login_name]["name"],
                    "surname": ldap_information[login_name]["surname"],
                    "home": ldap_information[login_name]["home"],
                    "shell":"/bin/bash",
                    "password_expire": ""
                    "groups": ldap_information[login_name]["groups"]
                },
                "machine-token": "a6d1abf7fcf04d5827db9b193a91254f915cba503a6f7f9c02a2bca05f2c8027"
            }
    except:
        status = False
    if status: 
        return json.dumps(data)
    else:
        return Response(response="Unauthorized",status=401) 

if __name__ == '__main__':
    api.run()
