#!/usr/bin/env python

import sys

from stringbuffer import StringBuffer
from math import pow

usage = """
Usage: {0} N K T 
  where 
  - N is the number of threads
  - K is the number of tables
  - T is the tax
"""

def mk_begin_context(N,K,T):
    sb = StringBuffer()
    sb.append("lfht_{0}_{1}_{2}: CONTEXT =\n".format(N,K,T))
    sb.append("BEGIN\n")
    return str(sb)

def mk_end_context(): return "END"


def mk_type_definitions(N,K,T):
    sb = StringBuffer()
    sb.append("N : int = {0};\n".format(K))
    sb.append("T : int = {0};\n".format(T))
    sb.append("THRESHOLD : real = 6/10;\n");
    sb.append("NUM_THREADS: nat = {0};\n".format(N))
    rest = """
table_index: TYPE = [1 .. N];
descriptor: TYPE = [# num_entries : nat, num_to_migrate: nat, assimilated : bool #];
table: TYPE = ARRAY table_index OF descriptor;
empty_descriptor : descriptor = (# num_entries := 0, num_to_migrate := 0, assimilated := FALSE #);
empty_table : table = [ [i: table_index] empty_descriptor ];
thread: TYPE = [1 .. NUM_THREADS];
"""
    return str(sb) + rest
  
add_trans_system = """
add : MODULE = 
 BEGIN
 GLOBAL
  K : nat,
  old, new : table_index,
  pending : int,
  did_not_pay : int,
  paid_tax : int,
  revenue : int,
  posted : int,
  ht : table
 OUTPUT
  pc : [0 .. 3]
 INITIALIZATION
   pc = 0;
   pending = 0;
   did_not_pay = 0;
   paid_tax = 0;
   posted = 0;
   revenue = 0;
   K = 32;
   ht = empty_table;
   old = 1;
   new = 1
TRANSITION
[ pc = 0 and old < new and not ht[old].assimilated --> 
        ht' = (ht WITH [new].num_entries := ht[new].num_entries + min (T,ht[old].num_to_migrate)) WITH [old].num_to_migrate := ht[old].num_to_migrate - min (T,ht[old].num_to_migrate) WITH [old].assimilated := (ht[old].num_to_migrate <= T);
	pending' = pending + 1;
	paid_tax' = paid_tax + 1;
	revenue' = revenue + min (T,ht[old].num_to_migrate);
        pc' = 1
  []
  pc = 0 and not( old < new and not ht[old].assimilated) -->  pc' = 1; pending' = pending + 1;
  []
  pc = 1 and ht[new].num_entries < K -->
        ht'[new].num_entries = ht[new].num_entries + 1;
	pending' = pending - 1 ;
        posted' = posted + 1;
        pc' = 2  
  []
  pc = 1 and ht[new].num_entries >= K -->
	pending' = pending - 1 ;
	did_not_pay' = did_not_pay - 1;
	pc' = 0
  []
  pc = 2 and ht[new].num_entries > THRESHOLD*K and new < N --> 
       K' = 2 * K;
       old' = new;
       new' = new + 1;
       ht' = (ht WITH [new+1].num_entries := 0) WITH [new+1].num_to_migrate := 0 WITH [new+1].assimilated := FALSE WITH [new].num_to_migrate := ht[new].num_entries;
       did_not_pay' = pending;
       paid_tax' = 0;
       revenue' = 0;
       posted' = 0;
       pc' = 0
  []
  pc = 2 and ht[new].num_entries <= THRESHOLD*K --> pc' = 0
  []
  pc = 2 and ht[new].num_entries > THRESHOLD*K  and new >= N --> pc' = 3
  []
  pc = 3 --> pc' = 3 
]

END;
"""

def mk_table_size_def(N):
    sb = StringBuffer()
    sb.append("table_size(i: table_index, k:nat): bool =\n")
    k = 5 ## 2^5 = 32
    for i in range(N): 
        sb.append("(i={0} and k={1})".format(i+1,int(pow(2,k+i))))
        if i < N-1:
            sb.append(" OR ")
        else:
            sb.append(";\n")
    return str(sb)    

def mk_full_table(N):
    sb = StringBuffer()
    sb.append("full_table(i: table_index, k:nat): bool =\n")
    k = 5 ## 2^5 = 32
    for i in range(N):
        sb.append("(i={0} and k>={1} * THRESHOLD)".format(i+1,int(pow(2,k+i))))
        if i < N-1:
            sb.append(" OR ")
        else:
            sb.append(";\n")
    return str(sb)    

def mk_max_size(N):
    sb = StringBuffer()
    sb.append("max_size(i: table_index): nat =\n")
    k = 5 ## 2^5 = 32
    for i in range(N):
        if i == 0:
            sb.append("IF i={0} THEN {1}\n".format(i+1,int(pow(2,k+i))))
        elif i < N-1:
            sb.append("ELSIF i={0} THEN {1}\n".format(i+1,int(pow(2,k+i))))
        elif i == N-1:
            sb.append("ELSE {0}\n".format(int(pow(2,k+i))))
    sb.append("ENDIF;\n")
    return str(sb)    

def mk_helpers(N):
    sb = StringBuffer()
    sb.append("min (a : nat, b: nat): nat = IF  a < b THEN a ELSE b ENDIF;\n")
    sb.append("Migrated(ht: table, i: table_index): nat = (ht[i].num_entries - ht[i].num_to_migrate);\n")
    sb.append(mk_table_size_def(N))
    sb.append("\n")
    sb.append(mk_full_table(N))
    sb.append("\n")
    sb.append(mk_max_size(N))
    sb.append("\n")
    return str(sb)

asyn_composition =  """
system: MODULE = WITH OUTPUT pc: ARRAY thread OF [0 .. 3]
      ([] (i: thread): (RENAME pc TO pc[i] IN add));
"""

def mk_pending_lemma(N,Filename):
    sb = StringBuffer()
    sb.append("% PROVED\n% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} pending\n".format(Filename))
    sb.append("pending: LEMMA system |- G( pending =")
    for i in range(N):
        sb.append("(IF pc[{0}] = 1 THEN 1 ELSE 0 ENDIF)".format(i+1))
        if i < N-1:
            sb.append(" + ")
    sb.append(");\n")
    return str(sb)

def mk_lemmas(N,Filename):

    lemmas = """
% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} lemma1
lemma1: LEMMA system |- G((new = 1 and old = 1) or (new = old+1));

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} lemma2 -l lemma1
lemma2: LEMMA system |- G(FORALL (i: table_index): i > new => ht[i].num_entries = 0 and ht[i].num_to_migrate=0 and not ht[i].assimilated);

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} lemma2a
lemma2a: LEMMA system |- G(FORALL (i: table_index): NOT full_table(i, ht[i].num_entries) AND table_size(i, ht[i].num_entries) => FORALL (j: table_index): j > i => ht[j].num_entries = 0 and ht[j].num_to_migrate=0 and not ht[j].assimilated);

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} lemma3
lemma3: LEMMA system |- G(FORALL (i: table_index): ht[i].num_to_migrate <= ht[i].num_entries);

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} lemma4
lemma4: LEMMA system |- G(table_size (new, K));

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} lemma5a -l lemma1 -l lemma4
lemma5a: LEMMA system |- G(NOT ht[new].assimilated);

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} lemma5b -l lemma1 -l lemma4
lemma5b: LEMMA system |- G(ht[new].num_to_migrate = 0);

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} lemma5c -l lemma1 -l lemma4
lemma5c: LEMMA system |- G(old = new OR full_table(old, ht[old].num_entries));

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} lemma5e -l lemma4 -l lemma5c
lemma5e: LEMMA system |- G(FORALL (i: table_index): NOT full_table(i, ht[i].num_entries) => NOT ht[i].assimilated);

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} lemma5f -l lemma4
lemma5f: LEMMA system |- G(FORALL (i: table_index): NOT full_table(i, ht[i].num_entries) => ht[i].num_to_migrate=0);

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} lemma5g -l lemma1 -l lemma2 -l lemma3 -l lemma4  -l lemma5a -l lemma5b -l lemma5c -l lemma5e -l lemma5f 
lemma5g: LEMMA system |- G(FORALL (i: table_index): ht[i].assimilated => ht[i].num_to_migrate=0);

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} final
final : LEMMA
    system |- G((EXISTS (i: thread): pc[i] = 3) => new = N);

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} lemma6 -l lemma1 -l lemma2 -l lemma3 -l lemma4 -l lemma5a -l lemma5b -l lemma5c -l lemma5e -l lemma5f -l lemma5g 
lemma6 : LEMMA
    system |- G(FORALL (i: table_index):
	           i <= old AND old < new =>
		        (full_table(i, ht[i].num_entries) => (ht[i].num_to_migrate = 0 <=> ht[i].assimilated)) AND
                        (NOT full_table(i, ht[i].num_entries) => NOT (ht[i].assimilated AND ht[i].num_to_migrate = 0)));


% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} lemma8b -l lemma1 -l lemma2 -l lemma3 -l lemma4 -l lemma5a -l lemma5b -l lemma5c -l lemma5e -l lemma5f -l lemma5g -l lemma6 
lemma8b : LEMMA
    system |- G((old < new AND Migrated(ht,old) = 0 AND ht[new].num_entries = 0) => 
	 	        (EXISTS (i: thread): pc[i] = 0));

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} pending_lb -l pending
pending_lb : LEMMA system |- G( pending >= 0 );

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} did_not_pay_ub -l pending_lb -l pending
did_not_pay_ub : LEMMA
      system |- G( did_not_pay < NUM_THREADS);		

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} paid_tax_0
paid_tax_0 : LEMMA
      system |- G( old = new => paid_tax = 0 );		
% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} paid_tax_1 -l pending_lb -l pending
paid_tax_1 : LEMMA
      system |- G( paid_tax >= 0);		

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} revenue_0
revenue_0 : LEMMA
      system |- G( old = new => revenue = 0 );		

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} revenue_1 -l pending_lb -l pending
revenue_1 : LEMMA
      system |- G( revenue >= 0);		

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} revenue_2 
revenue_2 : LEMMA
      system |- G( old < new =>  revenue = Migrated(ht,old));		

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} revenue_3 -l revenue_0 -l revenue_1 -l revenue_2
revenue_3 : LEMMA
      system |- G( old < new => revenue <= ht[old].num_entries);		

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} revenue_4 -l revenue_0 -l revenue_1 -l revenue_2 -l revenue_3
revenue_4 : LEMMA
      system |- G( old < new AND ht[old].num_to_migrate > 0 => revenue = T * paid_tax);

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} revenue_5 -l revenue_0 -l revenue_1 -l revenue_2 -l revenue_3 -l revenue_4
revenue_5 : LEMMA
      system |- G( old < new AND ht[old].num_to_migrate = 0 => revenue = ht[old].num_entries);

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} posted_0 -l pending_lb -l pending
posted_0 : LEMMA
      system |- G( posted >= 0);		

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} posted_1 -l posted_0 -l paid_tax_0 -l paid_tax_1 -l pending_lb -l pending -l lemma1 -l lemma2 -l lemma2a -l lemma3 -l lemma4  -l lemma5a -l lemma5b -l lemma5c -l lemma5e -l lemma5f -l lemma5g
posted_1 : LEMMA
      system |- G( old < new AND not ht[old].assimilated => 
	           posted + pending =  paid_tax + did_not_pay);		
		       

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} upper_strengthening_2 -l pending_lb -l pending -l lemma1 -l lemma2 -l lemma2a -l lemma3 -l lemma4 -l lemma5a -l lemma5b -l lemma5c -l lemma5e -l lemma5f -l lemma5g
upper_strengthening_2 :  LEMMA
       system |- G( (old < new => ht[new].num_entries <= T * paid_tax + posted));


% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} upper_strengthening_3 -l pending_lb -l pending  -l lemma1 -l lemma2 -l lemma2a -l lemma3 -l lemma4 -l lemma5a -l lemma5b -l lemma5c -l lemma5e -l lemma5f -l lemma5g
upper_strengthening_3 :  LEMMA
       system |- G( (old < new AND ht[old].num_to_migrate >= T => ht[new].num_entries = T * paid_tax + posted));

% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} upper_strengthening_3a -l pending_lb -l pending  -l lemma1 -l lemma2 -l lemma2a -l lemma3 -l lemma4 -l lemma5a -l lemma5b -l lemma5c -l lemma5e -l lemma5f -l lemma5g
upper_strengthening_3a :  LEMMA
       system |- G( (old < new => ht[new].num_entries <= T * paid_tax + posted));

% PROVED 
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} upper_strengthening_5 -l pending_lb -l pending -l lemma1 -l lemma2 -l lemma2a -l lemma3 -l lemma4 -l lemma5a -l lemma5b -l lemma5c -l lemma5e -l lemma5f -l lemma5g 
upper_strengthening_5 :  LEMMA
       system |- G( (old < new => ht[new].num_entries = Migrated(ht, old) + posted));

% PROVED 
% sal-inf-bmc -i -ice -v 1 -d 15 -s yices2 {0} upper_global -l pending_lb -l pending -l lemma1 -l lemma2 -l lemma2a -l lemma3 -l lemma4 -l lemma5a -l lemma5b -l lemma5c -l lemma5e -l lemma5f -l lemma5g -l lemma6 -l lemma8b -l upper_strengthening_2 -l upper_strengthening_3 -l upper_strengthening_3a -l paid_tax_0 -l paid_tax_1 -l posted_0 -l posted_1 -l did_not_pay_ub  -l upper_strengthening_5 -l revenue_0 -l revenue_1 -l revenue_2 -l final -l revenue_3 -l revenue_4 -l revenue_5
upper_global :  LEMMA
      system |- G( (FORALL (i: table_index): i < N => ht[i].num_entries <= (THRESHOLD * max_size(i)) + NUM_THREADS));
		   

% PROVED  
% sal-inf-bmc -i -ice -v 1 -d 15 -s yices2 {0} upper -l pending_lb -l pending  -l lemma1 -l lemma2 -l lemma2a -l lemma3 -l lemma4 -l lemma5a -l lemma5b -l lemma5c -l lemma5e -l lemma5f -l lemma5g -l lemma6 -l lemma8b -l upper_strengthening_2 -l upper_strengthening_3 -l upper_strengthening_3a -l paid_tax_0 -l paid_tax_1 -l posted_0 -l posted_1 -l did_not_pay_ub -l upper_strengthening_5 -l revenue_0 -l revenue_1 -l revenue_2 -l final -l revenue_3 -l revenue_4 -l revenue_5 -l upper_global
upper :  LEMMA
      system |- G( (old < new  AND not ht[old].assimilated  =>  
                    (ht[new].num_entries <= Migrated(ht,old) + (Migrated(ht,old) / T) +  (NUM_THREADS-1) + pending)));

""".format(Filename)
    sb = StringBuffer()
    sb.append(mk_pending_lemma(N,Filename))
    sb.append(lemmas)
    return str(sb)

def mk_properties(Filename):
    properties = """
% PROVED
% sal-inf-bmc -i -ice -v 1 -d 1 -s yices2 {0} prop1 -l upper -l upper_global  -l pending_lb -l pending  -l lemma1 -l lemma2 -l lemma2a -l lemma3 -l lemma4 -l lemma5a -l lemma5b -l lemma5c -l lemma5e -l lemma5f -l lemma5g 
prop1 : LEMMA system |- G(FORALL (i: table_index): i < old => ht[i].assimilated);
""".format(Filename)
    return properties

def main(args):
    if len(args) != 4:
        print usage.format(args[0])
        return 1
    else:
        N = int(args[1])
        K = int(args[2])
        T = int(args[3])

        if N < 1:
            print "ERROR: number of threads must be equal or greater than 1"
            sys.exit(1)
        if T < 1:
            print "ERROR: number of migrated entries must be equal or greater than 1"
            sys.exit(1)
        if K < 2:
            print "ERROR: number of tables must be equal or greater than 2"
            sys.exit(1)
            
        filename = "lfht_{0}_{1}_{2}.sal".format(N,K,T)
        with open(filename, 'w') as sal:
            sal.write(mk_begin_context(N,K,T))
            sal.write(mk_type_definitions(N,K,T))
            sal.write(mk_helpers(K))
            sal.write(add_trans_system)
            sal.write(asyn_composition)
            sal.write(mk_lemmas(N,filename))
            sal.write(mk_properties(filename))
            sal.write(mk_end_context())
            
        return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
