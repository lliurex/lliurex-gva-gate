#!/usr/bin/python3

from flask import Flask, json

companies = [{"gid": 10003, "name": "students", "members": ["jaume","alu01","alu02"]}, {"gid": 10004, "name": "teachers","members":["quique"]}]

api = Flask(__name__)

@api.route('/get_groups', methods=['GET'])
def get_companies():
  return json.dumps(companies)

@api.route('/login', methods=['GET'])
def login():
  return "-- login --"

if __name__ == '__main__':
    api.run()
