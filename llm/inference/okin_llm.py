import os
import re
import subprocess

from openai import OpenAI
from openai.types.chat import ChatCompletionMessageParam

# ======================
# -- PATHS
# ======================

BASE_DIR        = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OKIN_BIN        = os.path.join(BASE_DIR, "../build", "okin")
PROMPT_DIR      = os.path.join(BASE_DIR, "../prompt")
ACTIVATION_FILE = os.path.join(PROMPT_DIR, "okin_activation.txt")

DATA_DIR      = os.path.join(BASE_DIR, "./model/data")
OUTPUT_FILE   = os.path.join(DATA_DIR, "llm_output.txt")

# ======================
# -- CONFIG
# ======================

LM_BASE_URL     = "http://192.168.12.202:1234/v1"
LM_API_KEY      = "lm-studio"
LM_MODEL        = "local-model"
MAX_RETRIES     = 3
RECURSE_PROMPT  = 1

# ======================
# -- PROMPTS
# ======================

INITIAL_PROMPT = (
        "Generate 10 Okin test cases covering diverse features and edge cases.\n"
        "Each case must be unique and non-trivial.\n\n"
        "SYNTAX & EXECUTION RULES (violations cause runtime errors):\n"
        "1. Variables: Declare ALL vars with opcode 2 before use\n"
        "   BAD:  64<X,Y,Z> (Y,Z not declared)\n"
        "   GOOD: 2<X,10>;2<Y,5>;2<Z,0>;64<X,Y,Z>\n"
        "2. DEST requirement: Arithmetic (64-68), STRING (208~), MATH (224~) need pre-declared DEST\n"
        "3. No inline math: Can't write 'Y-1' or 'X*2' directly, must use opcodes\n"
        "4. Loop variables: ALL vars used in loop body must exist before loop starts\n"
        "   BAD:  32<I,0,5,1|64<SUM,I,SUM>> (SUM undefined)\n"
        "   GOOD: 2<SUM,0>;32<I,0,5,1|64<SUM,I,SUM>>\n"
        "5. WHILE safety: Variable must change each iteration or loop infinitely\n"
        "6. No comments: Remove all # comments from code\n"
        "7. Output requirement: Every test case must end with 192~WRITELN to print result\n\n"
        "REQUIRED COVERAGE (at least one example of each):\n"
        "- Arithmetic ops: ADD(64), SUB(65), MUL(66), DIV(67), MOD(68)\n"
        "- Control flow: FOR(32) loop, WHILE(33) loop, IF(112) conditional\n"
        "- String lib: LEN, CONCAT, SLICE, FIND, UPPER, or LOWER\n"
        "- Array ops: INDEX(50), SET_IDX(51), or IN(49) membership\n"
        "- Math lib: POW, SQRT, ABS, MIN, MAX, FLOOR, or CEIL\n"
        "- Functions: Define with FUNCTION(16), invoke with CALL(17)\n"
        "- Comparisons: Use EQ(80), NEQ(81), GT(82), LT(83), GTE(84), LTE(85)\n\n"
        "QUALITY STANDARDS:\n"
        "- No duplicates or minor variations of the same logic\n"
        "- Test cases should demonstrate realistic use patterns\n"
        "- Avoid trivial operations (e.g., just printing a constant)\n"
        "- Mix simple and complex examples\n\n"
        "FORMAT FOR EACH TEST CASE:\n"
        "TASK: [One sentence explaining what this code does]\n"
        "```okin\n"
        "[Code]\n"
        "```\n"
        )

RECURSE_MSG = (
        "Generate 10 additional Okin test cases, completely different from previous ones.\n"
        "Explore new feature combinations and edge cases not yet covered.\n"
        "Follow all syntax rules from the original prompt.\n\n"
        "OUTPUT FORMAT:\n"
        "TASK: [Description]\n"
        "```okin\n"
        "[Code]\n"
        "```\n"
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
    """
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

        content = stream_completion(messages)
        if content is None:
            print("[no response]")
            continue

        all_responses.append(content)

        messages.append({"role": "assistant", "content": content})
        messages.append({"role": "user",      "content": RECURSE_MSG})

    with open(OUTPUT_FILE, "w") as f:
        f.write("\n\n".join(all_responses))

    print(f"\nWritten to {OUTPUT_FILE}")
    """
    main()
