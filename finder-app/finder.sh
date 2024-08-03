#!/bin/sh
# Shell script for finder which finds number of files in directory as well as number of matching lines including error checking
# Author: Alec Ippolito


if [ "$#" -ne 2 ]
then
	echo "Error: 2 arrguments expected as [filesdir] [searchstr] but were not provided."
	exit 1
fi

filesdir="$1"
searchstr="$2"

# Check if file exists
if [ ! -d "$filesdir" ]
then
	echo "Error: File directory "$filesdir" does not exist."
	exit 1 
fi

num_files=$(find "$filesdir" -type f | wc -l)
num_lines=$(grep -r "$searchstr" "$filesdir" | wc -l)

echo "The number of files are  "$num_files" and the number of matching lines are "$num_lines"s"



