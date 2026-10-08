(
var path = thisProcess.argv[0];
var baselinePath = thisProcess.argv[1];
var file = SoundFile.new;
var data, sampleRate, frames, channels, baseline;
var expected = [220, 440, 880, 220, 440, 880, 220, 440, 880, 660, 660, 660, 660, 220, 880];
var check = { |condition, message|
	if(condition.not) { ("FAIL: " ++ message).postln; 1.exit };
};
check.(file.openRead(path), "cannot open rendered audio");
check.(file.numChannels == 19, "expected nineteen channels");
sampleRate = file.sampleRate;
frames = file.numFrames;
data = FloatArray.newClear(frames * 19);
file.readData(data);
file.close;
check.(data.every { |sample| sample.isNaN.not and: { sample.abs < 2 } }, "audio must stay finite and bounded");
// Examine two seconds after the FFT warmup. Upward zero crossings measure pitch.
channels = Array.fill(19, { |channel|
	FloatArray.fill((sampleRate * 2).asInteger, { |i| data[((i + (sampleRate * 0.5).asInteger) * 19) + channel] })
});
expected.do { |freq, channel|
	var samples = channels[channel];
	var crossings = 0;
	var measured;
	check.(samples.abs.maxItem > 0.001, "silent channel " ++ channel);
	(samples.size - 1).do { |i|
		if((samples[i] <= 0) and: { samples[i + 1] > 0 }) { crossings = crossings + 1 };
	};
	measured = crossings / 2;
	check.((measured - freq).abs < 4, "wrong pitch on channel " ++ channel ++ ": " ++ measured);
	("PITCH_OK " ++ channel ++ " " ++ measured).postln;
};
[15, 17, 18].do { |channel| check.(channels[channel].abs.maxItem > 0.001, "silent dynamic/extreme-size channel") };
check.(channels[16].abs.maxItem == 0, "silent input must remain silent");
// One branch passes through polar conversion; allow its lookup-table error.
check.((channels[11] - channels[12]).abs.maxItem < 0.001, "PV branching corrupted the shared spectrum");
if(baselinePath.notNil) {
	check.(file.openRead(baselinePath), "cannot open baseline audio");
	check.(file.numChannels == 3 and: { file.numFrames == frames }, "incompatible baseline");
	baseline = FloatArray.newClear(frames * 3);
	file.readData(baseline);
	file.close;
	3.do { |channel|
		var delta = 0.0;
		frames.do { |i| delta = delta.max((data[(i * 19) + channel] - baseline[(i * 3) + channel]).abs) };
		check.(delta < 0.000001, "audio rename changed channel " ++ channel ++ " by " ++ delta);
	};
	"BELLA_AUDIO_RENAME_EQUIVALENT".postln;
};
"BELLA_AUDIO_OK".postln;
0.exit;
)
