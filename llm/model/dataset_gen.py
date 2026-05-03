import os
import re
import json
import subprocess

# ======================
# -- PATHS
# ======================

BASE_DIR        = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OKIN_BIN        = os.path.join(BASE_DIR, "../build", "okin.exe")

DATA_DIR        = os.path.join(BASE_DIR, "model/data")
DATASET_FILE    = os.path.join(DATA_DIR, "okin_dataset.jsonl")

LLM_OUTPUT      = os.path.join(DATA_DIR, "llm_output.txt")

# ======================
# -- EXTRACTION & RUNNING
# ======================

def run_okin(code: str) -> tuple[bool, str]:
    code = code.strip().replace("\n", ";").replace('\\"', '"').replace("\\'", "'")
    result = subprocess.run([OKIN_BIN, code], capture_output=True, text=True)

    if result.returncode != 0:
        return False, result.stderr.strip()

    return True, result.stdout.strip()

def extract_blocks(llm_output: str) -> list[str]:
    results = []
    chunks = re.split(r"(TASK: .+)", llm_output)
    current_task = ""

    for chunk in chunks:
        task_match = re.fullmatch(r"TASK: (.+)", chunk.strip())
        if task_match:
            current_task = task_match.group(1)
            continue

        for block in re.findall(r"```okin\n(.*?)```", chunk, re.DOTALL):
            lines = [l.strip() for l in block.splitlines() if l.strip()]
            for line in lines:
                results.append((current_task, line))

    return results

def parse_and_run(llm_output: str) -> list[dict]:
    results = []
    for task, code in extract_blocks(llm_output):
        success, output = run_okin(code)
        results.append({"task": task, "code": code, "success": success, "output": output})

    return results

# ======================
# -- SAVE
# ======================

def save_dataset(results: list[dict], system_prompt: str = "", task: str = "") -> None:
    os.makedirs(DATA_DIR, exist_ok=True)

    with open(DATASET_FILE, "a") as f:
        for r in results:
            if not r["success"]:
                continue

            entry = {
                "messages": [
                    {"role": "system",    "content": system_prompt},
                    {"role": "user",      "content": r["task"]},
                    {"role": "assistant", "content": f"```okin\n{r['code']}\n```"},
                ],
                "okin_output": r["output"],
            }

            f.write(json.dumps(entry) + "\n")

# ======================
# -- INIT
# ======================

if __name__ == "__main__":
    results = parse_and_run(open(LLM_OUTPUT).read())
    save_dataset(results)