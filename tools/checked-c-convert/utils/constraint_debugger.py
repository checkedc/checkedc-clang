"""
Simple Constraint Debugging script.

This is a simple help script based on IPython
 to debug the constraint system of the
converter tool.

How to use it:
   lets say, if you want to know how the constraint variable "q_11858"
   got assigned to WILD.

   $ python constraint_debugger.py constraint_output.json
   ...
   In [1]: how_wild_recurse("q_11858")
   Out[1]:
          ['q_11858->',
           u'q_317953->',
           u'q_11815<-',
           u'q_305961<-',
           u'q_11811->',
           u'q_317927<-',
           u'q_909->',
           'WILD']
    '->' and '<-' shows whether the corresponding variable is in the LHS
    or RHS of the equality constraint.

    // if you want to know how "q_11858" got assigned to "ARR", then:
    In [2]: how_wild_recurse("q_11858", "ARR")
    Out[2]: []

    Which means that there is no path i.e., "q_11858" never got assigned to "ARR".

    You can get the description of a constraint variable (q_305961) as follows:
    In [3]: desc_cons("q_305961")
    Out[3]:
          {'desc': u'tmp_passwdfile',
          'line': u'/home/machiry/checkedc/benchmarks/icecast-2.4.4/src/auth_htpasswd.c:317:11'}


"""
import json
import sys


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


fwd_map = dict()
rev_map = dict()

fp = open(sys.argv[1], "r")
jobj = json.loads(fp.read())
fp.close()

# load the provided json in fwd and rev_map
for curr_d in jobj["Setup"]["Constraints"]:
  if "Eq" in curr_d.keys():
    curr_d = curr_d["Eq"]
    atom1 = curr_d["Atom1"]
    atom2 = curr_d["Atom2"]
    if atom1 not in fwd_map:
      fwd_map[atom1] = set()
    fwd_map[atom1].add(atom2)

    if atom2 != "WILD" and atom2 != "ARR" and atom2 != "PTR" and atom2 != "NTARR":
      if atom2 not in rev_map:
        rev_map[atom2] = set()
      rev_map[atom2].add(atom1)

total_cons_vars = {}
for curr_c_var in jobj["ConstraintVariables"]:
  li = curr_c_var["line"]
  for pvar in curr_c_var["Variables"]:
    all_vals = parse_ptr_var(pvar)
    for curr_k in all_vals:
      total_cons_vars[curr_k] = {}
      total_cons_vars[curr_k]['line'] = li
      total_cons_vars[curr_k]['desc'] = all_vals[curr_k]


how_wild_cache = {}


def desc_cons(curr_e):
  if curr_e in total_cons_vars:
    return total_cons_vars[curr_e]
  else:
    return "None"

def how_wild_recurse(curr_e, target_tag="WILD", visited=None):
  if visited is None:
    visited = set()
  if curr_e in visited:
    return []
  visited.add(curr_e)
  cache_key = (curr_e, target_tag)
  if cache_key in how_wild_cache:
    return how_wild_cache[cache_key]
  map_dirs = (("->", fwd_map), ("<-", rev_map))
  for dir_char, map in map_dirs:
    if curr_e in map:
      if target_tag in map[curr_e]:
        visited.remove(curr_e)
        res = [curr_e + dir_char, target_tag]
        how_wild_cache[cache_key] = res
        return res
      for cc in map[curr_e]:
        ccr = how_wild_recurse(cc, target_tag, visited)
        if len(ccr) > 0:
          visited.remove(curr_e)
          res = [curr_e + dir_char] + ccr
          how_wild_cache[cache_key] = res
          return res
  visited.remove(curr_e)
  return []

import IPython
IPython.embed()





