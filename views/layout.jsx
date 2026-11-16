import Lex from "@lek-js/lex"

const Layout = ({ children }) =>
{
	return <html>
		<head>
		<meta charset="UTF-8" />
		<meta name="viewport" content="width=device-width, initial-scale=1" />
		<title>LilyDev</title>
		<meta name="description" content="Monitor de partituras LilyPond en el navegador" />
	</head>
		<body>{children}</body>
	</html>
}

export default Layout;
