import math
import sys

def rms_utilization_bound(n: int) -> float:
    return n * (2 ** (1 / n) - 1)

def generate_table(max_n: int = 20):
    print("double rms_utilization_table[] = {")
    for i in range(0, max_n):
        if i == 0:
            print("    0.000000,  // n = 0, dont use")
            continue
        u = rms_utilization_bound(i)
        print(f"    {u:.6f},  // n = {i}")
    print("    0.693147  // n -> ∞ (ln(2))")
    print("};")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 rms_table_gen.py <max_n>")
        sys.exit(1)

    try:
        max_n = int(sys.argv[1])
    except ValueError:
        print("Error: <max_n> must be an integer.")
        sys.exit(1)

    if max_n <= 0:
        print("Error: <max_n> must be positive.")
        sys.exit(1)
    generate_table(max_n)