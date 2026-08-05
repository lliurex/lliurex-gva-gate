from json import dumps
from json import load as json_load
from pathlib import Path
from re import match as re_match

from llxgvagate.mapper import CdcMapper


class User:

    def __init__(self, login) -> None:
        clean_login = login.split("@")[0]
        self.login = clean_login
        self.name = ""
        self.surname = ""
        self.home = f"/home/{clean_login}"
        self.shell = "/bin/bash"
        self.uid = -1
        self.gid = -1
        self.groups = []
        self.load_types_user()

    def load_types_user(self):
        self.types_user = {}
        for filetype in Path("/usr/share/cdc-mapper/types").glob("*.json"):
            data = json_load(filetype.open())
            self.types_user[data["type"]] = data["regex"]

    def new_populate_user(self):
        user_types = []
        temp_groups = {}
        for x in self.groups:
            for key, value in self.types_user.items():
                if re_match(value, x.name.lower()):
                    user_types.append(key)
        cdcmapper = CdcMapper()
        aux_groups = cdcmapper.get_groups(user_types)
        for x in aux_groups:
            g = Group(x["name"], x["gid"])
            if "default_gid" in x:
                g.default_gid = x["default_gid"]
            temp_groups[g.name] = g
        max_id = 0
        for item in temp_groups.values():
            self.groups.append(item)
        for x in self.groups:
            if x.default_gid > -1 and x.default_gid > max_id:
                max_id = x.default_gid
                self.gid = x

    def __str__(self) -> str:
        return dumps(self.__dict__, 
                     default=lambda o: o.__dict__,
                     indent=4, 
                     ensure_ascii=False)


class Group:
    def __init__(self, name, gid) -> None:
        self.name = name
        self.gid = gid
        self.default_gid = -1

    def __str__(self) -> str:
        return dumps(self.__dict__, indent=4, ensure_ascii=False)
