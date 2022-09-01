#!/bin/bash

bitcodesPath=$1
numBitcodes=`ls -laSr $bitcodesPath/*.bc | wc -l`
if [[ $numBitcodes -lt 1 ]]; 
then
    echo "Number of bitcodes is less than 1"
    exit 1
fi

smallestBitcode=`ls -Sr $bitcodesPath/*.bc | head -n 1`
bitcodeBasename=`basename $smallestBitcode`

echo "Bitcodes count: $numBitcodes"
echo "Smallest bitcode: $smallestBitcode"
echo "Basename: $bitcodeBasename"

hast=`echo "$bitcodeBasename" | cut -d "_" -f 1`
echo "Hast: $hast"
dmeta=`ls $bitcodesPath/*${hast}*.metad`
echo "Dmeta: $dmeta"

/home/meetesh/BUILDS/OBAP/deprecate.sh $dmeta $smallestBitcode