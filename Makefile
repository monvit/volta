hooks:
	chmod +x scripts/hooks/pre-commit
	cd scripts/hooks && ln -sfr ./pre-commit ../../.git/hooks/pre-commit
