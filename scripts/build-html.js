const build = require("@lek-js/lex/build-html");
const path = require("path");
const fs = require("fs");
const mainPath = path.resolve(process.cwd(), "views/main.jsx");
const htmlDirPath = path.resolve(process.cwd(), "html");
const outPath = path.resolve(process.cwd(), "html/index.html");

build(mainPath, { minify: true }).then((out) =>
{
	if(out.error) throw out.error;
	fs.mkdirSync(htmlDirPath, { recursive: true });
	fs.writeFileSync(outPath, out.htmlText);

	console.log("HTML is builded");
}).catch(console.error);
