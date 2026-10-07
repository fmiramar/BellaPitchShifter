PV_PitchShiftBella : UGen {
	*ar { |in = 0.0, pitchRatio = 1.0, fftSize = 1024, hopSize = 128, mul = 1.0, add = 0.0|
		^this.multiNew('audio', in, pitchRatio, fftSize, hopSize).madd(mul, add)
	}

	checkInputs {
		^this.checkSameRateAsFirstInput
	}
}
