rm -rf /var/tmp/*.so
rm -rf pdbRoot*
rm -rf pdbRoot
rm -rf /mnt/pdbRoot*
rm -rf /tmp/CatalogDir
rm -rf CatalogDir/*
rm -rf CatalogDir*
rm -rf /tmp/CatalogDir*
rm -rf logs/*
rm -rf /mnt1/data/*
rm -rf /mnt2/data/*
rm -rf /mnt1/tmp/*
rm -rf /mnt2/tmp/*
pkill -9 pdb-server
pkill -9 pdb-cluster
pkill -9 test603
pkill -9 test404