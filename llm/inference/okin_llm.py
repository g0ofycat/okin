import os
import re
import subprocess

from openai import OpenAI
from openai.types.chat import ChatCompletionMessageParam

# ======================
# -- PATHS
# ======================

BASE_DIR        = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OKIN_BIN        = os.path.join(BASE_DIR, "../build", "okin.exe")
PROMPT_DIR      = os.path.join(BASE_DIR, "../prompt")
ACTIVATION_FILE = os.path.join(PROMPT_DIR, "okin_activation.txt")

DATA_DIR      = os.path.join(BASE_DIR, "./model/data")
OUTPUT_FILE   = os.path.join(DATA_DIR, "llm_output.txt")

# ======================
# -- CONFIG
# ======================

LM_BASE_URL     = "http://localhost:1234/v1"
LM_API_KEY      = "lm-studio"
LM_MODEL        = "local-model"
MAX_RETRIES     = 3
RECURSE_PROMPT  = 1

# ======================
# -- PROMPTS
# ======================

INITIAL_PROMPT = (
    "Write 10 Okin test cases.\n"
    "You are encouraged to be creative and cover a wide range of features and edge cases, but make sure to follow the rules. Do not use comments in your code\n\n"
    "CRITICAL RULES (violations will cause runtime errors):\n"
    "- INLINE MATH: Code like 'Y-1, X+1, Z*2' isn't valid, you must use the opcode for specific arithmetic operations\n"
    "- ALL variables must be declared with opcode 2 BEFORE they are used\n"
    "- WRONG: 64<X, Y, Z> before declaring Y and Z\n"
    "- CORRECT: 2<X,10>;2<Y,5>;2<Z,0>;64<X,Y,Z>;192~WRITELN<Z>\n"
    "- DEST in arithmetic (64-68), string (208~), math (224~) must be declared first\n"
    "- FOR loops: all vars used inside the body must be declared before the loop\n"
    "- WRONG: 32<I,0,5,1|64<SUM,I,SUM>> without declaring SUM first\n"
    "- CORRECT: 2<SUM,0>;32<I,0,5,1|64<SUM,I,SUM>>;192~WRITELN<SUM>\n"
    "- WHILE loops need a variable that changes inside the body or they loop forever\n"
    "- Each test case must end with 192~WRITELN to print the result\n"
    "- One test case per line, no duplicates (DON'T OVERFIT, DON'T WRITE SIMILAR TEST CASES THAT ONLY HAVE 1 THING CHANGED)\n\n"
    "Coverage requirements (at least 5 of each):\n"
    "- Basic arithmetic: ADD, SUB, MUL, DIV, MOD with different values each time\n"
    "- FOR loops: different ranges, steps, and accumulation patterns\n"
    "- WHILE loops: different conditions\n"
    "- IF conditionals: GT, LT, EQ, NEQ, GTE, LTE\n"
    "- String ops: LEN, CONCAT, SLICE, FIND, UPPER, LOWER on different strings\n"
    "- Array ops: AGET, ASET, IN on different arrays\n"
    "- Math lib: POW, SQRT, ABS, MIN, MAX, FLOOR, CEIL\n"
    "- Functions: FUNCTION + CALL with at least 2 different functions\n\n"
    "Output only the okin code block."
)

RECURSE_MSG = (
    "Write 10 more Okin test cases, different from the ones above.\n"
    "Output only the okin code block."
)

# ======================
# -- SETUP
# ======================

client = OpenAI(base_url=LM_BASE_URL, api_key=LM_API_KEY)

def load_prompts() -> str:
    with open(ACTIVATION_FILE) as f:
        activation = f.read()

    parts = [activation]
    for fname in sorted(os.listdir(PROMPT_DIR)):
        if fname.endswith(".txt") and fname != "okin_activation.txt":
            with open(os.path.join(PROMPT_DIR, fname)) as f:
                parts.append(f.read())

    return "\n\n".join(parts)

# ======================
# -- OKIN
# ======================

def run_okin(code: str) -> tuple[bool, str]:
    code = code.strip().replace("\n", ";").replace('\\"', '"').replace("\\'", "'")
    result = subprocess.run([OKIN_BIN, code], capture_output=True, text=True)

    if result.returncode != 0:
        return False, result.stderr.strip()

    return True, result.stdout.strip()

def fix_okin_block(code: str, error: str, messages: list[ChatCompletionMessageParam]) -> str | None:
    fix_messages = messages + [{
        "role": "user",
        "content": (
            f"Your Okin code failed:\n"
            f"```okin\n{code}\n```\n"
            f"Error: {error}\n\n"
            f"Rules:\n"
            f"- Every variable must be declared with opcode 2 before use\n"
            f"- DEST in arithmetic (64-68) must be pre-declared: 2<R, 0>;64<A, B, R>\n"
            f"- Write the entire fixed program as a single line\n"
            f"- Return only the corrected okin code block, nothing else"
        )
    }]

    print("\n[retrying okin fix...]\n", flush=True)

    response = stream_completion(fix_messages)
    if response is None:
        return None

    blocks = extract_okin_blocks(response)
    return blocks[0] if blocks else None

def extract_okin_blocks(text: str) -> list[str]:
    programs = []

    for block in re.findall(r"```okin\n(.*?)```", text, re.DOTALL):
        lines = [l.strip() for l in block.splitlines() if l.strip()]
        programs.extend(lines)

    return programs

def process_okin_blocks(response: str, messages: list[ChatCompletionMessageParam]) -> str:
    blocks = extract_okin_blocks(response)
    if not blocks:
        return response

    for block in blocks:
        code = block
        success, output = run_okin(code)

        for _ in range(MAX_RETRIES):
            if success:
                break

            print(f"\n[okin error] {output}", flush=True)
            fixed = fix_okin_block(code, output, messages)

            if fixed is None:
                break

            code = fixed
            success, output = run_okin(code)

        label = "[okin output]" if success else "[okin failed after retries]"
        response += f"\n{label} {output}"
        print(f"\n{label} {output}", flush=True)

    return response

# ======================
# -- CHAT
# ======================

def stream_completion(messages: list[ChatCompletionMessageParam]) -> str | None:
    stream = client.chat.completions.create(
            model=LM_MODEL,
            messages=messages,
            stream=True,
            )

    chunks: list[str] = []
    for chunk in stream:
        delta = chunk.choices[0].delta.content
        if delta:
            print(delta, end="", flush=True)
            chunks.append(delta)

    print()

    content = "".join(chunks)
    return content or None

def chat(messages: list[ChatCompletionMessageParam]) -> str | None:
    content = stream_completion(messages)
    if content is None:
        return None

    return process_okin_blocks(content, messages)

def main():
    system_prompt = load_prompts()
    messages: list[ChatCompletionMessageParam] = [
            {"role": "system", "content": system_prompt}
            ]

    print("Okin LLM ready. Type 'exit' to quit\n")

    while True:
        user_input = input("You: ").strip()
        if not user_input:
            continue
        if user_input.lower() == "exit":
            break

        messages.append({"role": "user", "content": user_input})
        print("\nLLM: ", end="", flush=True)

        response = chat(messages)
        if response is None:
            print("[no response]")
            continue

        messages.append({"role": "assistant", "content": response})

        print()

# ======================
# -- INIT
# ======================

if __name__ == "__main__":
    system_prompt = load_prompts()
    os.makedirs(DATA_DIR, exist_ok=True)

    messages: list[ChatCompletionMessageParam] = [
        {"role": "system", "content": system_prompt},
        {"role": "user", "content": INITIAL_PROMPT},
    ]

    all_responses: list[str] = []

    for i in range(RECURSE_PROMPT):
        print(f"\n[{i + 1} / {RECURSE_PROMPT}]\n")
        print("LLM: ", end="", flush=True)

        response = chat(messages)
        if response is None:
            print("[no response]")
            continue

        all_responses.append(response)

        messages.append({"role": "assistant", "content": response})
        messages.append({"role": "user", "content": RECURSE_MSG})

    with open(OUTPUT_FILE, "w") as f:
        f.write("\n\n".join(all_responses))

    print(f"\nWritten to {OUTPUT_FILE}")