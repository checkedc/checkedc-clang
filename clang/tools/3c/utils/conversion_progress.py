import sys
import json

"""
Script that tries to find SCC in the constraint graph.
It tries to find the impact factor of each pointer.
i.e., Identify all the pointers which have been explicitly marked WILD
      and find all other pointer that are affected by the given pointer.
"""


"""
Simple implementation of union-find data structure. 
"""
class DisjointSet(object):

  def __init__(self):
    self.leader = {}  # maps a member to the group's leader
    self.group = {}  # maps a group leader to the group (which is a set)

  def add(self, a, b):
    leadera = self.leader.get(a)
    leaderb = self.leader.get(b)
    if leadera is not None:
      if leaderb is not None:
        if leadera == leaderb: return  # nothing to do
        groupa = self.group[leadera]
        groupb = self.group[leaderb]
        if len(groupa) < len(groupb):
          a, leadera, groupa, b, leaderb, groupb = b, leaderb, groupb, a, leadera, groupa
        groupa |= groupb
        del self.group[leaderb]
        for k in groupb:
          self.leader[k] = leadera
      else:
        self.group[leadera].add(b)
        self.leader[b] = leadera
    else:
      if leaderb is not None:
        self.group[leaderb].add(a)
        self.leader[a] = leaderb
      else:
        self.leader[a] = self.leader[b] = a
        self.group[a] = set([a, b])


def parse_pointer_var(ptr_json):
  all_vars = {}
  var_name = ptr_json['name']
  for cvar in ptr_json['Vars']:
    all_vars[cvar] = var_name
  return all_vars


def parse_function_ptr_var(ptr_json):
  all_vars = {}
  for cptr in  ptr_json['Parameters']:
    for cval in cptr:
      childv = parse_ptr_var(cval)
      all_vars.update(childv)

  for rvar in ptr_json['ReturnVar']:
    childv = parse_ptr_var(rvar)
    all_vars.update(childv)
  return all_vars


def parse_ptr_var(json_obj):
  ret_val = {}
  if 'FunctionVar' in json_obj:
    ret_val = parse_function_ptr_var(json_obj['FunctionVar'])
  if 'PointerVar' in json_obj:
    ret_val = parse_pointer_var(json_obj['PointerVar'])
  return ret_val


ptrs_set = dict()
real_wild_ptrs = set()
all_sets = list()
all_atoms = set()

fp = open(sys.argv[1], "r")
jobj = json.loads(fp.read())
fp.close()
group_index = dict()
group_nums = 1
ds = DisjointSet()
# load the provided json in fwd and rev_map
for curr_d in jobj["Setup"]["Constraints"]:
  if "Eq" in curr_d.keys():
    curr_d = curr_d["Eq"]
    atom1 = curr_d["Atom1"]
    atom2 = curr_d["Atom2"]
    all_atoms.add(atom1)
    all_atoms.add(atom2)

    if atom2 != "WILD" and atom2 != "ARR" and atom2 != "PTR" and atom2 != "NTARR":
      ds.add(atom1, atom2)
    else:
      if atom2 == "WILD":
        real_wild_ptrs.add(atom1)

# print("All Sets:" + str(len(all_sets)))
# print("All Atoms:" + str(len(all_atoms)))
# print("All Wilds:" + str(len(real_wild_ptrs)))
all_sets = list(ds.group.values())

all_non_indi = set()
for cus in all_sets:
  all_non_indi.update(cus)


all_wild = set()
src_ptr_wild = set()
non_complete_sets = list()
complete_sets = list()

total_wild_added = 0
for currs in all_sets:
  wild_sets = currs.intersection(real_wild_ptrs)
  if len(wild_sets) > 0:
    all_wild.update(currs)
    src_ptr_wild.update(wild_sets)
    total_wild_added += len(wild_sets)
    print("Marked Wild:" + str(len(currs)) + ", Actual Wild:" + str(len(wild_sets)))
    if len(wild_sets) == len(currs):
      complete_sets.append((currs, wild_sets))
    else:
      non_complete_sets.append((currs, wild_sets))

assert(len(src_ptr_wild) == total_wild_added and "Sanity of union finding algo")

print("Total Wild Pointers:" + str(len(all_wild)) + ", Wild Pointers that are explicitly marked:" + str(len(src_ptr_wild)))
print("Number of constrain graphs that contains at least one non-explicitly marked wild pointer:" + str(len(non_complete_sets)))
print("Number of constrain graphs where all the pointers are explicitly marked:" + str(len(complete_sets)))
impact_maps = dict()
for curr_nc_t, curr_c_w in non_complete_sets:
  cur_r = len(curr_nc_t) / len(curr_c_w)
  if cur_r not in impact_maps:
    impact_maps[cur_r] = list()
  impact_maps[cur_r].append((curr_nc_t, curr_c_w))

print("Impact map (gain ratio, number of sets):")
for curr_f in impact_maps:
  print(str(curr_f) + ":" + str(len(impact_maps[curr_f])))
