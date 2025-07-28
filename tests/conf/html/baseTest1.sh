for i in $(seq 0 2); do
    curl -s http://127.0.0.1:808$i > tmp$i \
        && diff tmp$i server$((i+1))/index.html \
        && rm tmp$i;
done
