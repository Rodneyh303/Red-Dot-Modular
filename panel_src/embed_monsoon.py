import sys, math
sys.path.insert(0, "panel_src")
from embed_components import emit

params = [
    *[(f"SEMI{i}_PARAM", 7.5+i*9.0, 59.75) for i in range(12)],
    ("OCT_LO_PARAM",119.0,59.75),("OCT_HI_PARAM",128.0,59.75),
    ("MODE_PARAM",197.0,12.0),
    ("DICE_R_PARAM",6.0,84.0),("DICE_M_PARAM",17.0,84.0),
    ("DICE_TRIAL_R_PARAM",6.0,92.0),("DICE_TRIAL_M_PARAM",17.0,92.0),
    ("DICE_SLEW_R_PARAM",6.0,76.0),("DICE_SLEW_M_PARAM",17.0,76.0),
    ("RHYTHM_MIX_PARAM",6.0,100.0),("MELODY_MIX_PARAM",17.0,100.0),
    ("LOCK_PARAM",148.0,87.0),("MUTE_PARAM",163.0,87.0),
    ("RESET_BUTTON_PARAM",178.0,87.0),("RUN_GATE_PARAM",193.0,87.0),
    ("NOTE_VALUE_PARAM",16.0,22.0),("VARIATION_PARAM",42.0,22.0),("LEGATO_PARAM",68.0,22.0),
    ("REST_PARAM",94.0,22.0),("ACCENT_KNOB",120.0,22.0),
    ("BPM_PARAM",148.0,58.0),("PATTERN_LENGTH_PARAM",163.0,58.0),("PATTERN_OFFSET_PARAM",178.0,58.0),
]
inputs = [
    ("RUN_GATE_INPUT",15.0,105.0),("RESET_TRIGGER_INPUT",32.0,105.0),("SEED_INPUT",49.0,105.0),
    ("GATE1_INPUT",66.0,105.0),("GATE2_INPUT",83.0,105.0),("GATE3_MOD_INPUT",100.0,105.0),
    ("CLK_INPUT",15.0,120.0),("LENGTH_INPUT",32.0,120.0),("OFFSET_INPUT",49.0,120.0),
    ("CV1_INPUT",66.0,120.0),("CV2_INPUT",83.0,120.0),("CV3_MOD_INPUT",100.0,120.0),
]
outputs = [
    ("GATE_OUTPUT",114.0,105.0),("TIE_OUTPUT",131.0,105.0),("LEGATO_OUTPUT",148.0,105.0),
    ("TIE_OR_LEGATO_OUTPUT",165.0,105.0),("ACCENT_OUTPUT",182.0,105.0),
    ("CV_OUTPUT",114.0,120.0),("SEED_OUTPUT",131.0,120.0),("RUN_GATE_OUTPUT",148.0,120.0),
    ("RESET_TRIGGER_OUTPUT",165.0,120.0),
]
RCX,RCY,RLED=162.0,30.0,14.0
ring=[(f"STEP_LIGHT_{i}", RCX+RLED*math.cos(i/16*2*math.pi-math.pi/2),
       RCY+RLED*math.sin(i/16*2*math.pi-math.pi/2)) for i in range(16)]
lights=[*ring,
    *[(f"MODE_A_LIGHT_{i}",192.0,34.0+i*8.0) for i in range(4)],
    ("RHYTHM_DICE_LIGHT",9.2,80.8),("MELODY_DICE_LIGHT",20.2,80.8),("LOCK_LIGHT",148.0,93.0),
    ("MUTE_LIGHT",163.0,93.0),("RESET_LIGHT",178.0,93.0),("RUN_GATE_LIGHT",193.0,93.0),
    ("SCALE_EXPANDER_LIGHT",4.0,4.0),("DNA_EXPANDER_LIGHT",9.0,4.0),("POLY_EXPANDER_LIGHT",14.0,4.0),
]
for v in ["dark","light"]:
    n = emit(f"res/panels/Monsoon_panel_{v}_monsoon.svg", params, inputs, outputs, lights)
    print(f"Monsoon {v}: {n} components embedded")
