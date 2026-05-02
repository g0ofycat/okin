import os
import re
import subprocess

from openai import OpenAI
from openai.types.chat import ChatCompletionMessageParam

# ======================
# -- CONFIG
# ======================

BASE_DIR        = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OKIN_BIN        = os.path.join(BASE_DIR, "build", "okin.exe")
PROMPT_DIR      = os.path.join(BASE_DIR, "prompt")
ACTIVATION_FILE = os.path.join(PROMPT_DIR, "okin_activation.txt")
LM_BASE_URL     = "http://localhost:1234/v1"
LM_API_KEY      = "lm-studio"
LM_MODEL        = "local-model"

# ======================
# -- SETUP
# ======================

client = OpenAI(base_url=LM_BASE_URL, api_key=LM_API_KEY)

def load_prompts():
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

def run_okin(code: str) -> str:
    code = code.strip().replace("\n", ";")
    code = code.replace('\\"', '"').replace("\\'", "'")
    result = subprocess.run(
            [OKIN_BIN, code],
            capture_output=True,
            text=True
            )
    if result.returncode != 0:
        return f"[okin error] {result.stderr.strip()}"
    return result.stdout.strip()

def process_response(response: str) -> str:
    blocks = re.findall(r"```okin\n(.*?)```", response, re.DOTALL)
    if not blocks:
        blocks = re.findall(r"(?:^|\n)okin\n(.*?)(?:\n\n|$)", response, re.DOTALL)
    for block in blocks:
        output = run_okin(block)
        response += f"\n[okin output] {output}"

    return response

# ======================
# -- CHAT
# ======================

def chat(messages: list[ChatCompletionMessageParam]) -> str | None:
    completion = client.chat.completions.create(
            model=LM_MODEL,
            messages=messages
            )

    content = completion.choices[0].message.content
    if content is None:
        return None

    return process_response(content)

def main():
    system_prompt = load_prompts()
    messages: list[ChatCompletionMessageParam] = [
            {"role": "system", "content": system_prompt}
            ]

    print("Okin LLM ready. Type 'exit' to quit.\n")

    while True:
        user_input = input("You: ").strip()
        if not user_input:
            continue
        if user_input.lower() == "exit":
            break

        messages.append({"role": "user", "content": user_input})
        response = chat(messages)
        if response is None:
            print("\nLLM: [no response]\n")
            continue

        messages.append({"role": "assistant", "content": response})
        print(f"\nLLM: {response}\n")

if __name__ == "__main__":
    main()
