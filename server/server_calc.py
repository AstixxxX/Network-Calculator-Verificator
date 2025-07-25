import sys

expr = sys.argv[1]

try:
    result = eval(expr.replace("^", "**"))
    print(result)
except Exception as e:
    sys.exit(1)