case $1 in
    '-n')
        cat VERSION
        ;;
    '-c')
        cat VERSION | awk -F '.' '{print 10000 * $1 + 100 * $2 + $3}'
        ;;
    *)
        echo 'error arg'
        exit 1
        ;;
esac