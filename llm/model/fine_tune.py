import os
import sys
import torch
import subprocess

from datasets import load_dataset
from peft import LoraConfig
from transformers import AutoTokenizer, AutoModelForCausalLM
from trl import SFTTrainer, SFTConfig

# ======================
# -- PATHS
# ======================

MODEL_ID   = "Qwen/Qwen2.5-Coder-1.5B-Instruct"
DATASET_ID = "g0ofycatz/OkinDataset"

BASE_DIR    = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUTPUT_DIR  = os.path.join(BASE_DIR, "./okin_llm")

# ======================
# -- PORT & QUANTIZATION
# ======================

GGUF_DIR    = os.path.join(BASE_DIR, "./gguf")
LLAMA_CPP   = os.path.join(BASE_DIR, "../../llama.cpp")
# QUANT_TYPE  = "Q4_K_M"

# ======================
# -- DATA
# ======================

dataset   = load_dataset(DATASET_ID, split="train")
tokenizer = AutoTokenizer.from_pretrained(MODEL_ID)

# ======================
# -- MODEL
# ======================

model = AutoModelForCausalLM.from_pretrained(
    MODEL_ID,
    dtype=torch.float32,
    device_map="cpu",
    low_cpu_mem_usage=True,
)

lora_config = LoraConfig(
    r=16,
    lora_alpha=32,
    target_modules=["q_proj", "v_proj", "k_proj", "o_proj",
                    "gate_proj", "up_proj", "down_proj"],
    lora_dropout=0.05,
    task_type="CAUSAL_LM",
)

# ======================
# -- TRAIN
# ======================

args = SFTConfig(
    output_dir=OUTPUT_DIR,
    gradient_checkpointing=True,
    dataloader_pin_memory=False,
    auto_find_batch_size=True,
    assistant_only_loss=True,
    fp16=True,
    gradient_checkpointing_kwargs={"use_reentrant": False},
    per_device_train_batch_size=2,
    gradient_accumulation_steps=4,
    num_train_epochs=5,
    learning_rate=2e-4,
    logging_steps=10,
    save_strategy="epoch",
    warmup_steps=10,
    lr_scheduler_type="cosine",
    max_length=512,
    eos_token="<|im_end|>",
)

trainer = SFTTrainer(
    model=model,
    peft_config=lora_config,
    processing_class=tokenizer,
    args=args,
    train_dataset=dataset,
)

trainer.train()

merged = trainer.model.merge_and_unload()
merged.save_pretrained(OUTPUT_DIR)
tokenizer.save_pretrained(OUTPUT_DIR)

# ======================
# -- GGUF
# ======================

os.makedirs(GGUF_DIR, exist_ok=True)

gguf_fp16    = os.path.join(GGUF_DIR, "okin-qwen-fp16.gguf")
# gguf_final = os.path.join(GGUF_DIR, f"okin-qwen-{QUANT_TYPE.lower()}.gguf")
convert      = os.path.join(LLAMA_CPP, "convert_hf_to_gguf.py")
# quantize   = os.path.join(LLAMA_CPP, "build", "bin", "llama-quantize.exe")

print("Converting to GGUF...")
subprocess.run([sys.executable, convert, OUTPUT_DIR, "--outfile", gguf_fp16, "--outtype", "f16"], check=True)

"""
print(f"Quantizing to {QUANT_TYPE}...")
subprocess.run([quantize, gguf_fp16, gguf_final, QUANT_TYPE], check=True)
"""

# os.remove(gguf_fp16)

print(f"Done: {gguf_fp16}")