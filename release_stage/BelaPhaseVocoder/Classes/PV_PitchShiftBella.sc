PV_PitchShiftBella : PV_ChainUGen {
	*new { |chain, pitchRatio = 1.0|
		^this.multiNew('control', chain, pitchRatio)
	}

	checkInputs {
		if(inputs[0].isKindOf(PV_ChainUGen).not) {
			^"chain must be an FFT or PV-chain UGen"
		};
		^nil
	}
}
