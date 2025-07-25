expr=$(echo "$1" | sed -e 's/+/%2B/g' \
                       -e 's/\*/%2A/g' \
                       -e 's/\//%2F/g' \
                       -e 's/(/%28/g' \
                       -e 's/)/%29/g' \
                       -e 's/!/%21/g')

curl --silent \
     -X POST \
     -H "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/138.0.0.0 Safari/537.36" \
     -d "function=$expr" \
     -d "result_type=number" \
     -d "_p1=2331" \
     "https://ru.numberempire.com/expressioncalculator.php" | grep -oP '<span id=result1>\K[^<]*' | tr -d '\n'> answer_client.txt
