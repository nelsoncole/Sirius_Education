# ============================================================================
#        Project: Sirius_Education
#       Filename: Makefile
#    Description: Gestor central de compilação e instalação de módulos (LKM).
# 
#         Author: Nelson Cole
#   Created Date: 19/09/2026
# ============================================================================
MODULES = sample_mod e1000

# Caminhos do Sistema de Build
BUILD_DIR = build
SYSROOT_DIR = ../sysroot/mods

.PHONY: all clean install $(MODULES)

# Regra padrão: compila todos os módulos
all: $(MODULES)

$(MODULES):
	@$(MAKE) -C $@

# NEW: Regra de instalação para mover os binários .ko para a sysroot
install: all
	@mkdir -p $(SYSROOT_DIR)
	@cp $(BUILD_DIR)/*.ko $(SYSROOT_DIR)/
	@echo "[LKM Central] Modulos copiados com sucesso de '$(BUILD_DIR)/' para '$(SYSROOT_DIR)/'."

clean:
	@for mod in $(MODULES); do \
		$(MAKE) -C $$mod clean; \
	done
	@rm -rf $(BUILD_DIR)/*.ko
	@echo "[LKM Central] Pasta mods/build/ limpa com sucesso."
