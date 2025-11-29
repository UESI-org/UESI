#!/bin/sh -

umask 007

VERSION_FILE="version"

if [ ! -r "${VERSION_FILE}" -o ! -s "${VERSION_FILE}" ]
then
	echo 0 > "${VERSION_FILE}"
fi

v=$(cat "${VERSION_FILE}")

u=$(logname)              # Username of builder
d=${PWD%/obj}            # Build directory (strip /obj suffix if present)
h=$(hostname)            # Build hostname
t=$(date)                # Build timestamp
id=$(basename "${d}")    # Kernel config name (e.g., GENERIC)

ost="UESI"
osr="0.1"

cat >vers.c <<EOF
const char ostype[] = "${ost}";
const char osrelease[] = "${osr}";
const char osversion[] = "${id}#${v}";
const char sccs[] =
    "    @(#)${ost} ${osr} (${id}) #${v}: ${t}\n";
const char version[] =
    "${ost} ${osr} (${id}) #${v}: ${t}\n    ${u}@${h}:${d}\n";
EOF

expr ${v} + 1 > "${VERSION_FILE}"

echo "Generated vers.c: ${ost} ${osr} (${id}) #${v}"