#!/usr/bin/env python



times = { 'glibc'      :  [271, 378, 232, 200, 374, 354, 429, 318, 443, 299, 305, 180 ],
          'psmalloc2'  :  [331, 376, 260, 208, 373, 356, 421, 315, 443, 423, 307, 513 ],
          'elfpa'      :  [517, 403, 388, 227, 403, 386, 472, 327, 489, 286, 313, 156 ],
          'sri-glibc'  :  [336, 409, 259, 241, 404, 384, 463, 331, 481, 437, 334, 232 ],
          }
          

benchmarks = [
    '400.perlbench ',
    '401.bzip2     ',
    '403.gcc       ',
    '429.mcf       ',
    '445.gobmk     ',
    '456.hmmer     ',
    '458.sjeng     ',
    '462.libquantum',
    '464.h264ref   ',
    '471.omnetpp   ',
    '473.astar     ',
    '483.xalancbmk ',
]


versions = [ 'psmalloc2', 'elfpa', 'sri-glibc']



def stats(version, base):
    t = times[version]
    g = times[base]
    print '\n%%{0} overhead, in %,  compared to {1}:'.format(version, base)
    for i in range(12):
        percent = ((t[i] - g[i]) * 100)/ g[i]
        print  '%%{0}\t:\t{1}'.format(benchmarks[i],  percent)


for version in ['elfpa', 'psmalloc2', 'sri-glibc']:
    stats(version, 'glibc')

