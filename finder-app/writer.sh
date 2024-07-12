#!/bin/bash
# Shell script for writing content to a fil with error checking
# Author: Alec Ippolito

if [ "$#" -ne 2 ]
then
	echo "Error: 2 arrguments expected as [filesdir] [searchstr] but were not provided."
	exit 1
fi

writefile="$1"
writestr="$2"

mkdir -p "$(dirname "$writefile")"
echo "$writestr" > "$writefile" || {
	echo "Error: Failed to create fle $writefile."
	exit 1
}

echo "File created sucessfully: $writefile"
