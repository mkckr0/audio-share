case $1 in
    '-n')
        cat VERSION
        ;;
    '-c')
        cat VERSION | awk -F '.' '{print 1000000 * $1 + 1000 * $2 + $3}'
        ;;
    *)
        echo 'error arg'
        exit 1
        ;;
esac