(
var destination = thisProcess.argv[0];
var baseline = thisProcess.argv[1] == "baseline";
var audioClass = if(baseline) { \PV_PitchShiftBella.asClass } { \PitchShiftBella.asClass };
var definition = SynthDef(\bellaVerification, {
	var src = SinOsc.ar(440, 0, 0.05);
	var ratios = [0.5, 1, 2];
	var signals = ratios.collect { |ratio| audioClass.perform(\ar, src, ratio, 2048, 256) };
	if(baseline.not) {
		var polar, downstream, fork, expanded;
		signals = signals ++ ratios.collect { |ratio|
			IFFT.ar(PV_PitchShiftBella(FFT(LocalBuf(2048), src, 0.125, 1), ratio), 1)
		};
		signals = signals ++ ratios.collect { |ratio|
			IFFT.ar(PV_PitchShiftBella(FFT(LocalBuf(1024), src, 0.25, 1), ratio), 1)
		};
		polar = PV_MagAbove(FFT(LocalBuf(2048), src, 0.125, 1), 0);
		downstream = PV_PitchShiftBella(FFT(LocalBuf(2048), src, 0.125, 1), 1.5);
		fork = PV_PitchShiftBella(FFT(LocalBuf(2048), src, 0.125, 1), 1.5);
		expanded = PV_PitchShiftBella(FFT(LocalBuf(1024), src, 0.25, 1), [0.5, 2]);
		signals = signals ++ [
			IFFT.ar(PV_PitchShiftBella(polar, 1.5), 1),
			IFFT.ar(PV_MagAbove(downstream, 0), 1),
			IFFT.ar(fork, 1),
			IFFT.ar(PV_MagAbove(fork, 0), 1)
		] ++ IFFT.ar(expanded, 1) ++ [
			IFFT.ar(PV_PitchShiftBella(FFT(LocalBuf(1024), src, 0.25, 1), Line.kr(0.5, 2, 2.5)), 1),
			IFFT.ar(PV_PitchShiftBella(FFT(LocalBuf(1024), DC.ar(0), 0.25, 1), 2), 1),
			IFFT.ar(PV_PitchShiftBella(FFT(LocalBuf(8192), src, 0.125, 1), 1.5), 1),
			IFFT.ar(PV_PitchShiftBella(FFT(LocalBuf(256), src, 0.5, 1), 1.5), 1)
		];
	};
	Out.ar(0, signals);
});
Score([
	[0, [\d_recv, definition.asBytes]],
	[0, [\s_new, \bellaVerification, 1000, 0, 1]],
	[3, [\n_free, 1000]],
	[3.2, [0]]
]).writeOSCFile(destination, 0, 3.2);
"BELLA_SCORE_OK".postln;
0.exit;
)
