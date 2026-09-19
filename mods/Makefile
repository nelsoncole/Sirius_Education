# ============================================================================
#        Project: Sirius_Education
#       Filename: Makefile
#    Description: Makefile central da pasta mods. Coordena a compilacao
#                 e limpeza de todas as arvores de drivers LKMs.
# 
#         Author: Nelson Cole
#   Created Date: 19/09/2026
# ============================================================================

# Lista de pastas de drivers ativos (adiciona novos aqui separados por espaco)
MODULES = sample_mod

.PHONY: all clean $(MODULES)

all: $(MODULES)

$(MODULES):
	@$(MAKE) -C $@

clean:
	@for mod in $(MODULES); do \
		$(MAKE) -C $$mod clean; \
	done
	rm -rf build/*.ko
	@echo "[LKM Central] Pasta mods/build/ limpa com sucesso."
