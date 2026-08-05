import grp
import json
from pathlib import Path

from murmurhash import mrmr


class CdcMapper:

    STUDENTS = 1
    TEACHERS = 2
    ADMINS = 4

    def __init__(self) -> None:
        self.groups_folders = [
            Path("/usr/share/cdc-mapper/groups"),
            Path("/etc/cdc-mapper"),
        ]
        self.alu_groups = []
        self.doc_groups = []
        self.adm_groups = []

    def check_json(self, info):
        return "name" in info

    def get_groups(self, user_groups_type):
        groups = []
        for folder_path in self.groups_folders:
            if not folder_path.exists():
                continue
            for file_path in folder_path.iterdir():
                try:
                    with file_path.open("r") as fd:
                        group_info = json.load(fd)
                except Exception:
                    group_info = None
                if group_info is not None:
                    # Define default values for info object
                    aux = self.process_group(group_info, user_groups_type)
                    if aux is not None:
                        groups.append(aux)
        return groups

    def process_group(self, group_info, user_groups_type):
        if not self.check_json(group_info) or \
           not self.validate_in_group(group_info, user_groups_type):
            return None

        args = {"name": group_info["name"]}
        args = self.set_gid_from_name(args, group_info)
        if "gid" in group_info:
            args["default_id"] = group_info["gid"]
        if "default_gid" in group_info:
            args["default_gid"] = group_info["default_gid"]
        return args

    def validate_in_group(self, group_info, user_groups_type):
        if "types" in group_info:
            '''New format'''
            '''
                compact form to get all boolean values of the types in the
                group_info that are present in the user_groups_type
            '''
            all_groups_values = [group_info["types"][key] for key in user_groups_type if key in group_info["types"]]
        else:
            '''Old format'''
            valid_values = ["alu", "doc", "adm"]
            checked_groups = list(set(valid_values) & set(user_groups_type))
            all_groups_values = [group_info[key] for key in checked_groups if key in group_info]
        return any(all_groups_values)

    def set_gid_from_name(self, args, info):
        try:
            args["gid"] = grp.getgrnam(info["name"]).gr_gid
        except KeyError:
            if "gid" in info:
                args["gid"] = info["gid"]

        return args


class SSSDMapper:
    def __init__(self) -> None:
        self.rangesize = 200000
        self.maxslices = 10000
        self.idmap_lower = 200000

    def get_unix_uid_from_sid(self, sid):
        rid = self.get_rid_from_sid(sid)
        domain_sid = self.get_domain_sid(sid)
        first_rid = self.get_first_rid(self.rangesize, rid)
        aux_domain_sid = domain_sid
        if first_rid != 0:
            aux_domain_sid = domain_sid + "-" + str(first_rid)
        min_range = self.get_min_range(aux_domain_sid,
                                       self.rangesize,
                                       self.maxslices,
                                       self.idmap_lower)
        return min_range + (rid - first_rid)

    def get_rid_from_sid(self, sid):
        return int(sid.split('-')[-1])

    def get_first_rid(self, rangesize, rid):
        return int(rid / rangesize) * rangesize

    def get_domain_sid(self, sid):
        return '-'.join(sid.split('-')[0:-1])

    def get_min_range(self, sid, rangesize, maxslices, idmap_lower):
        hash_value = mrmr.hash(sid, 0xdeadbeef)
        new_slice = int(hash_value % maxslices)
        return (rangesize * new_slice) + idmap_lower

