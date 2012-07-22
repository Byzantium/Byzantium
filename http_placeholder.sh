echo  "Installing http service placeholders."
echo -e "\tInstalling microblog placeholder."
mkdir -p ${FAKE_ROOT}/srv/httpd/htdocs/microblog/
cat > ${FAKE_ROOT}/srv/httpd/htdocs/microblog/index.html << EOF
<html>
	<head>
		<title>NO PANTS!</title>
	</head>
	<body>
		<h2>NO PANTS!</h2>
		<p>you've caught us with our pants ... err ... services down ...</p>
		<p>the microblog functionality is going to be replaced by ejabberd (tenetively) plugins</p>
	</body>
</html>
EOF

