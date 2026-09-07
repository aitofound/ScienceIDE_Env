#!/bin/bash

orig_dir=$(pwd)
cd "$HOME" || exit 1

if [ -n "$1" ]; then
    group_arg="-W group_list=s$1"
else
    group_arg=""
fi

table=$(node_stats.sh | awk '
    /^Nodes used\/free by hardware type:/ { in_hw=1; next }
    /^GPUs used\/free/ { in_hw=0 }

    in_hw && /Cores/ {
        nodename=$1
        for (i=1; i<=NF; i++) {
            if ($i=="Cores") {
                cores[nodename]=$(i-1)
                break
            }
        }
    }

    /^Nodes currently allocated to the devel queue:/ {
        in_block=1
        next
    }

    in_block==1 && NF==0 { exit }

    in_block==1 {
        nodename=$1
        free=$NF
        data[nodename, "cores"]=cores[nodename]
        data[nodename, "free"]=free
        data[nodename, "total"]=cores[nodename]*free   # <-- new fourth column
        nodes[++count]=nodename
    }

    END {
        max_node_len=length("NODETYPE")
        max_cpu_len=length("CPUs / NODE")
        max_free_len=length("FREE NODES")
        max_total_len=length("TOTAL CPUs")  # <-- header for fourth column

        for (i=1; i<=count; i++) {
            n = nodes[i]
            if (length(n) > max_node_len) max_node_len=length(n)
            if (length(data[n,"cores"]) > max_cpu_len) max_cpu_len=length(data[n,"cores"])
            if (length(data[n,"free"]) > max_free_len) max_free_len=length(data[n,"free"])
            if (length(data[n,"total"]) > max_total_len) max_total_len=length(data[n,"total"])
        }

        printf "%-*s | %-*s | %-*s | %-*s\n", max_node_len, "NODETYPE", max_cpu_len, "CPUs / NODE", max_free_len, "FREE NODES", max_total_len, "FREE CPUs"
        printf "%s\n", gensub(/./,"-","g",sprintf("%*s", max_node_len+max_cpu_len+max_free_len+max_total_len+9,""))

        for (i=1; i<=count; i++) {
            n = nodes[i]
            printf "%-*s | %-*d | %-*s | %-*d\n", max_node_len, n, max_cpu_len, data[n,"cores"], max_free_len, data[n,"free"], max_total_len, data[n,"total"]
        }
    }')

echo
echo "$table"
echo

read -p "Please enter the NODETYPE: " nodetype
echo
read -p "How many nodes would you like to request? " nodereq

ncpus=$(echo "$table" | awk -F'|' -v nt="$nodetype" '{
    gsub(/^[ \t]+|[ \t]+$/, "", $1)
    gsub(/^[ \t]+|[ \t]+$/, "", $2)
    if ($1==nt) print $2
}')

echo
qsub -I -V -X -q devel $group_arg -lselect=${nodereq}:ncpus=${ncpus}:model=${nodetype},walltime=02:00:00

