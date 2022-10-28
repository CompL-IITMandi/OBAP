for dir in /home/meetesh/BUILDS/OBAP2/rawBC/bitcodesExp1/*/     # list directories in the form "/tmp/dirname/"
do
    dir=${dir%*/}      # remove the trailing "/"
    # echo "$dir"    # print everything after the final "/"
    ./run.sh $dir /home/meetesh/BUILDS/OBAP2/tmp/ >> out
done