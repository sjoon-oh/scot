from os import listdir
from os.path import isfile, join

import csv
import numpy

targets = [f for f in listdir(".") if isfile(join(".", f))]
excludes = ['CMakeLists.txt', 'CMakeCache.txt']

for fname in targets:
    if 'csv' in fname: 
        continue
    else: excludes.append(fname)

for fname in excludes:
    if fname in targets:
        targets.remove(fname)

summary = []
for fname in targets:
    print("{}: ".format(fname), end="")

    with open(fname, "r") as f:

        try: 
            lines = [line.rstrip() for line in f]
            lines = (line for line in lines if line)
            lines = [float(line.rstrip()) for line in lines]

            reqs = len(lines)

            avg = numpy.mean(lines)
            avg = round(avg, 2)
            tail_1 = lines[int(round(reqs * 0.01))]
            tail_50 = lines[int(round(reqs * 0.5))]
            tail_90 = lines[int(round(reqs * 0.9))]
            tail_95 = lines[int(round(reqs * 0.95))]
            tail_99 = lines[int(round(reqs * 0.99))]

            time = numpy.sum(lines)

            summary.append({
                "file": fname,
                "reqs": reqs,
                "time(s)": "{}".format(round(time * 0.000001, 2)),
                "avg(us)": round(avg, 2),
                "1th(us)": round(tail_1, 2),
                "50th(us)": round(tail_50, 2),
                "99th(us)": round(tail_99, 2)
            })

            print("ok.")

        except:
            print("unexpected format.")


with open("status-report.csv", "w") as f:
    writer = csv.DictWriter(f, 
        fieldnames=['file', 'reqs', 'time(s)', 'avg(us)', '1th(us)', '50th(us)', '99th(us)'], delimiter='\t')

    writer.writeheader()
    writer.writerows(summary)


