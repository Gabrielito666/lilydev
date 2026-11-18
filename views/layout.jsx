import Lex from "@lek-js/lex";
import "./styles.css";

const Layout = ({ children }) =>
{
	return <html>
		<head>
		<meta charset="UTF-8" />
		<link rel="icon" href="data:," />
		<meta name="viewport" content="width=device-width, initial-scale=1" />
		<title>LilyDev</title>
		<meta name="description" content="LilyPond score monitor in the browser" />
	</head>
		<body>{children}</body>
	</html>
}

export default Layout;
