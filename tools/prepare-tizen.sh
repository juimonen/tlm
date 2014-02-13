# $1 corresponds to tlm-<ver>.tar.gz 
# $2 is the destination folder
# NOTE: all the files will be extracted under destination folder (instead of destfolder/tlm-<version>)

if [ $# -ne 2 -o -z "$1" -o -z "$2" ]; then
    echo "Invalid arguments supplied"
    echo "Usage: ./prepare-tizen.sh /absolute/path/to/tlm-<version>.tar.gz /absolute/path/to/destfolder"
    echo "NOTE: All the files will be extracted under destfolder (instead of destfolder/tlm-<version>)"
    exit
fi

currdir=`pwd`;
echo "CURR dir = $currdir"

mkdir -p $2 && \
cd $2 && \
git rm -r *; rm -rf packaging;
tar -xzvf $currdir/$1 -C $2 --strip-components 1 && \
mkdir -p packaging && \
cp dists/rpm/tizen/packaging/tlm.spec packaging/ && \
cp dists/rpm/tizen/packaging/tlm.manifest packaging/ && \
cp dists/rpm/tizen/packaging/tlm.changes packaging/ && \
git add -f *;
cp $currdir/.gitignore $2/; 
