/* Função de callback que será chamada para CADA controlador USB detetado */
int meu_driver_usb_init(pci_device_t *dev)
{
    // Verifica a ProgIF (Interface de Programação) para saber o tipo exato de USB
    if (dev->prog_if == 0x30) {
        kprintf("   -> Inicializando como USB 3.0 (xHCI)\n");
        // iomap do dev->bar[0] e configuração dos registos xHCI...
        return 0; // Sucesso
    } 
    else if (dev->prog_if == 0x20) {
        kprintf("   -> Inicializando como USB 2.0 (EHCI)\n");
        // Configuração EHCI...
        return 0; // Sucesso
    }

    return -1; // Tipo de USB não suportado por este driver
}

void subsistema_usb_start(void)
{
    kprintf("[Kernel] Iniciando varrimento da pilha USB...\n");
    
    /* Classe 0x0C, Subclasse 0x03 = USB Controller */
    int total = pci_load_devices_by_class(0x0C, 0x03, meu_driver_usb_init);
    
    kprintf("[Kernel] Pilha USB carregada. %d controladores ativos no sistema.\n", total);
}
