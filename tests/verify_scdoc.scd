(
var target = thisProcess.argv[0];
var pages = ["Classes/PitchShiftBella", "Classes/PV_PitchShiftBella"];
var check = { |condition, message|
	if(condition.not) { ("FAIL: " ++ message).postln; 1.exit };
};
if(target.notNil) { SCDoc.helpTargetDir = target };
SCDoc.verbosity = 2;
SCDoc.indexAllDocuments(true);
pages.do { |path|
	var document = SCDoc.documents[path];
	var tree, source;
	check.(document.notNil, "missing help " ++ path);
	tree = SCDoc.parseDoc(document);
	check.(tree.notNil, "cannot parse " ++ path);
	SCDoc.renderer.renderToFile(document.destPath, document, tree);
	check.(File.exists(document.destPath), "cannot render " ++ path);
	source = File.readAllString(document.fullPath);
	source.findAll("code::\n").do { |offset|
		var start = offset + 7;
		var end = source.find("\n::", offset: start);
		check.(source.copyRange(start, end - 1).compile.notNil, "invalid example in " ++ path);
	};
	("RENDERED " ++ path).postln;
};
"BELLA_SCDOC_OK".postln;
0.exit;
)
