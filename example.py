from nbt2dict import parse_nbt
import gzip
import base64
import time
from pathlib import Path
import json

def load_example_nbt_data() -> list[str]:
    example_data_filename = "example_data.txt"

    with open(Path(__file__).parent / example_data_filename, "r") as f:
        return f.readlines()

def decode_time_and_print_nbt(nbt_data: str) -> None:
    nbt_data = base64.b64decode(nbt_data)
    nbt_data = gzip.decompress(nbt_data)

    start = time.time()
    parsed_nbt = parse_nbt(nbt_data)
    print(f"Parsing NBT took: {time.time() - start} seconds.")

    print(json.dumps(parsed_nbt, indent=2))


if __name__ == "__main__":
    example_nbt_data = load_example_nbt_data()
    for example_nbt_datapoint in example_nbt_data:
        decode_time_and_print_nbt(example_nbt_datapoint)
