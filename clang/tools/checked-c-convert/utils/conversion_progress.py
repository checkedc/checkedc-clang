import sys
import json

"""
Script that tries to find SCC in the constraint graph.
It tries to find the impact factor of each pointer.
i.e., Identify all the pointers which have been explicitly marked WILD
      and find all other pointer that are affected by the given pointer.
"""


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

# load the provided json in fwd and rev_map
for curr_d in jobj["Setup"]["Constraints"]:
  if "Eq" in curr_d.keys():
    curr_d = curr_d["Eq"]
    atom1 = curr_d["Atom1"]
    atom2 = curr_d["Atom2"]
    all_atoms.add(atom1)
    all_atoms.add(atom2)

    if atom2 != "WILD" and atom2 != "ARR" and atom2 != "PTR" and atom2 != "NTARR":
      if atom1 in ptrs_set:
        ptrs_set[atom1].add(atom2)
      elif atom2 in ptrs_set:
        ptrs_set[atom2].add(atom1)
      else:
        new_set = set()
        ptrs_set[atom1] = new_set
        ptrs_set[atom2] = new_set
        new_set.add(atom1)
        new_set.add(atom2)
        all_sets.append(new_set)
    else:
      if atom2 == "WILD":
        real_wild_ptrs.add(atom1)

print("All Sets:" + str(len(all_sets)))
print("All Atoms:" + str(len(all_atoms)))
print("All Wilds:" + str(len(real_wild_ptrs)))

all_non_indi = set()
for cus in all_sets:
  all_non_indi.update(cus)


all_wild = set()
src_ptr_wild = set()
non_complete_sets = list()
complete_sets = list()

for currs in all_sets:
  wild_sets = currs.intersection(real_wild_ptrs)
  if len(wild_sets) > 0:
    all_wild.update(currs)
    src_ptr_wild.update(wild_sets)
    # print("Orig:" + str(len(currs)) + ", Wild:" + str(len(wild_sets)))
    if len(wild_sets) == len(currs):
      complete_sets.append((currs, wild_sets))
    else:
      non_complete_sets.append((currs, wild_sets))

print("Total Wild:" + str(len(all_wild)) + ", Src Wild:" + str(len(src_ptr_wild)))
print("Total non complete sets:" + str(len(non_complete_sets)) + ", Complete sets:" + str(len(complete_sets)))
impact_maps = dict()
for curr_nc_t, curr_c_w in non_complete_sets:
  cur_r = len(curr_nc_t) / len(curr_c_w)
  if cur_r not in impact_maps:
    impact_maps[cur_r] = list()
  impact_maps[cur_r].append((curr_nc_t, curr_c_w))

print("Impact map:")
for curr_f in impact_maps:
  print(str(curr_f) + ":" + str(len(impact_maps[curr_f])))