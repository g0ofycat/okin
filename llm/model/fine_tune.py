# DATASET: https://huggingface.co/datasets/g0ofycatz/OkinDataset
# MODEL: https://huggingface.co/Qwen/Qwen2.5-Coder-1.5B-Instruct

import torch
import os

from datasets import load_dataset
from peft import LoraConfig
from transformers import AutoTokenizer, AutoModelForCausalLM
from trl import SFTTrainer, SFTConfig

MODEL_ID   = "Qwen/Qwen2.5-Coder-1.5B-Instruct"
DATASET_ID = "g0ofycatz/OkinDataset"

BASE_DIR   = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUTPUT_DIR = os.path.join(BASE_DIR, "./okin_llm")

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
    dtype=torch.bfloat16,
    device_map="cpu",
    low_cpu_mem_usage=True,
)

lora_config = LoraConfig(
    r=16,
    lora_alpha=32,
    target_modules=["q_proj", "v_proj", "k_proj", "o_proj"],
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
    gradient_checkpointing_kwargs={'use_reentrant': False},
    per_device_train_batch_size=2,
    gradient_accumulation_steps=4,
    num_train_epochs=3,
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

# ======================
# -- SAVE
# ======================

model.config.save_pretrained(OUTPUT_DIR)

trainer.save_model(OUTPUT_DIR)
tokenizer.save_pretrained(OUTPUT_DIR)

print(f"Saved to {OUTPUT_DIR}")