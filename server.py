#!/usr/bin/python3

from datetime import date
from flask import Flask, json, request, Response, jsonify

passwd = {"alu01" : "alu01secret"}

ldap_information = {"alu01":{"password":"alu01secret","gid":{"name":"Domain Users","gid":288400513}, "groups":[{"name":"sudo","gid":27},{"name":"teachers","gid":10003},{"name":"GRP_03000394","gid":288412920},{"name":"DenegarPermisosListadoAD","gid":74373983},{"name":"Docente","gid":288412920}],"uid":288430185,"name":"Alumno","surname":"Estudiante","home":"/home/alu01"}}

api = Flask(__name__)

@api.route('/api/v1/login', methods=['POST'])
def login():
    login_name = request.form.get("user")
    print(login_name)
    print(request.form.get("passwd"))
    try:
        status = passwd[login_name] == request.form.get("passwd")
        data = {"user":
                {
                    "login":login_name,
                    "uid":ldap_information[login_name]["uid"],
                    "gid":{"name":"Domain Users", "gid":288400513},
                    "name":ldap_information[login_name]["name"],
                    "surname": ldap_information[login_name]["surname"],
                    "home": ldap_information[login_name]["home"],
                    "shell":"/bin/bash",
                    "password_expire": "",
                    "groups": ldap_information[login_name]["groups"]
                },
                "machine_token": "a6d1abf7fcf04d5827db9b193a91254f915cba503a6f7f9c02a2bca05f2c8027"
            }
    except:
        status = False
    if status: 
        return jsonify(data)
    else:
        return Response(response="Unauthorized",status=401) 

if __name__ == '__main__':
    api.run()
