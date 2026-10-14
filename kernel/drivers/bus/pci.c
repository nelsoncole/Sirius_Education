/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: pci.c
 *    Description: Implementação do subsistema de barramento PCI.
 *                 Gere o varrimento recursivo do hardware, leitura/escrita
 *                 no Espaço de Configuração via portas I/O e registo de drivers.
 *                 Implementado via pool estático para evitar Page Faults.
 * 
 *         Author: Nelson Cole
 *   Created Date: 07/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 07/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

 #include <kernel/arch/x86_64/kapi/acpi.h>
#include <kernel/drivers/bus/pci.h>
#include <kernel/klib.h>

#define MAX_PCI_DEVICES_POOL 128

/* Pool estático global para armazenamento físico dos dispositivos detetados */
static pci_device_t g_pci_pool[MAX_PCI_DEVICES_POOL];
static int g_pci_pool_count = 0;

/* Cabeça da lista ligada lógica que interliga os dispositivos ativos */
static pci_device_t *g_pci_devices_head = NULL;

/*
 * Encapsulamento de funções inline de baixo nível para comunicação com portas do CPU.
 */
static inline void outl(uint16_t port, uint32_t val)
{
    __asm__ __volatile__("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port)
{
    uint32_t ret;
    __asm__ __volatile__("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/**
 * Lê uma Double Word (32 bits) do Espaço de Configuração PCI (Mecanismo nº 1).
 */
uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    uint32_t address;
    uint32_t lbus = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;

    /*
     * Monta o endereço de 32 bits para a porta PCI_CONFIG_ADDRESS:
     * Bit 31: Enable Bit (deve ser 1)
     * Bits 23-16: Bus Number
     * Bits 15-11: Device/Slot Number
     * Bits 10-8: Function Number
     * Bits 7-2: Register Offset (alinhado a 4 bytes, os 2 bits menos significativos são 0)
     */
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
                         (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));

    /* Envia o endereço pretendido para a porta de controlo */
    outl(PCI_CONFIG_ADDRESS, address);

    /* Lê e devolve o dado contido na porta de dados */
    return inl(PCI_CONFIG_DATA);
}

uint8_t pci_config_read_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    // Lê o dword completo (4 bytes) alinhado usando a tua função nativa
    uint32_t dword = pci_config_read_dword(bus, slot, func, offset);
    
    // Isola o byte correto dependendo do alinhamento (offset & 3)
    // offset % 4 == 0 -> de desloca 0 bits (byte 0)
    // offset % 4 == 1 -> de desloca 8 bits (byte 1)
    // offset % 4 == 2 -> de desloca 16 bits (byte 2)
    // offset % 4 == 3 -> de desloca 24 bits (byte 3)
    return (uint8_t)((dword >> ((offset & 3) * 8)) & 0xFF);
}

void pci_config_write_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t val)
{
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;

    // Monta o endereço padrão da porta PCI_CONFIG_ADDRESS (alinhado a 4 bytes com offset & 0xFC)
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
                         (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));

    outl(PCI_CONFIG_ADDRESS, address);

    // Lê o valor atual completo de 32 bits para modificar apenas o byte pretendido (Preservação)
    uint32_t current_dword = inl(PCI_CONFIG_DATA);
    
    // Calcula a máscara e a posição do bit-shift
    uint8_t shift = (offset & 3) * 8;
    uint32_t mask = ~(0xFF << shift);
    
    // Injeta o novo byte na posição correta sem corromper os outros 3 bytes
    uint32_t new_dword = (current_dword & mask) | ((uint32_t)val << shift);

    // Envia o dword modificado de volta
    outl(PCI_CONFIG_DATA, new_dword);
}

/**
 * Escreve uma Double Word (32 bits) no Espaço de Configuração PCI.
 */
void pci_config_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t data)
{
    uint32_t address;
    uint32_t lbus = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;

    address = (uint32_t)((lbus << 16) | (lslot << 11) |
                         (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));

    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, data);
}

/**
 * Adiciona um dispositivo detetado à lista ligada global do Kernel.
 */
static void pci_add_device(pci_device_t *dev)
{
    if (!g_pci_devices_head)
    {
        g_pci_devices_head = dev;
    }
    else
    {
        pci_device_t *current = g_pci_devices_head;
        while (current->next)
        {
            current = current->next;
        }
        current->next = dev;
    }
}

/**
 * Interroga e regista as propriedades de uma função específica de um dispositivo.
 */
static void pci_probe_function(uint8_t bus, uint8_t device, uint8_t function) 
{
    /* Lê o primeiro registo para extrair os IDs básicos */
    uint32_t reg0 = pci_config_read_dword(bus, device, function, PCI_REG_VENDOR_ID);
    uint16_t vendor_id = (uint16_t)(reg0 & 0xFFFF);
    uint16_t device_id = (uint16_t)((reg0 >> 16) & 0xFFFF);
    
    /* Se o Vendor ID for 0xFFFF, a função/dispositivo não existe */
    if (vendor_id == 0xFFFF) return;
    
    /* Proteção contra transbordo (overflow) do pool estático fixo */
    if (g_pci_pool_count >= MAX_PCI_DEVICES_POOL)
    {
        kprintf("[PCI] Erro: Pool estatico cheio. Ignorando dispositivo 0x%x\n", device_id);
        return;
    }

    /* Lê o registo de classe, subclasse e interface de programação */
    uint32_t reg8 = pci_config_read_dword(bus, device, function, PCI_REG_REVISION);
    uint8_t class_code    = (uint8_t)((reg8 >> 24) & 0xFF);
    uint8_t subclass_code = (uint8_t)((reg8 >> 16) & 0xFF);
    uint8_t prog_if       = (uint8_t)((reg8 >> 8)  & 0xFF);
    
    /* Lê a linha de interrupção física contida no registo 0x3C */
    uint32_t reg3c = pci_config_read_dword(bus, device, function, PCI_REG_INTERRUPT_LINE);
    uint8_t irq_line = (uint8_t)(reg3c & 0xFF);

    /* 
     * Resgata um nó pré-alocado do pool estático indexado.
     * MEDIDA ANTI-PAGE FAULT: Limpa o bloco inteiro, garantindo que .next é nulo
     * e desfazendo links circulares residuais remanescentes de reinicializações.
     */
    pci_device_t* dev = &g_pci_pool[g_pci_pool_count++];
    memset(dev, 0, sizeof(pci_device_t));

    /* Preenche os metadados estruturados */
    dev->address.bus = bus;
    dev->address.device = device;
    dev->address.function = function;
    dev->vendor_id = vendor_id;
    dev->device_id = device_id;
    dev->class_code = class_code;
    dev->subclass_code = subclass_code;
    dev->prog_if = prog_if;
    dev->irq_line = irq_line;
    dev->next = NULL; /* Assegura a interrupção da cauda da lista ligada */

    /* 
     * LEITURA EM LOOP DOS 6 BASE ADDRESS REGISTERS (BAR0 a BAR5)
     * Percorre os deslocamentos de 0x10 (BAR0) até 0x24 (BAR5) avançando de 4 em 4 bytes.
     */
    for (int i = 0; i < 6; i++)
    {
        dev->bar[i] = pci_config_read_dword(bus, device, function, PCI_REG_BAR0 + (i * 4));
    }

    /* Anexa o dispositivo mapeado à lista lógica do sistema */
    pci_add_device(dev);

    /*kprintf("[PCI] Descoberto -> Bus %d:Dev %d:Func %d | Vid:0x%x Did:0x%x | Classe:0x%x Sub:0x%x\n", 
            bus, device, function, vendor_id, device_id, class_code, subclass_code);*/
}

/**
 * Varre um dispositivo específico. Suporta a verificação de chips Multi-Função.
 */
static void pci_probe_device(uint8_t bus, uint8_t device)
{
    /* Verifica a primeira função (0) para determinar a existência do hardware */
    uint32_t reg0 = pci_config_read_dword(bus, device, 0, PCI_REG_VENDOR_ID);
    if ((uint16_t)(reg0 & 0xFFFF) == 0xFFFF)
        return;

    pci_probe_function(bus, device, 0);

    /* Lê o Tipo de Cabeçalho (Header Type) para saber se é multi-função */
    uint32_t regc = pci_config_read_dword(bus, device, 0, PCI_REG_CACHE_LINE);
    uint8_t header_type = (uint8_t)((regc >> 16) & 0xFF);

    /* Se o bit 7 do Header Type estiver ativo, o dispositivo possui múltiplas funções (0-7) */
    if (header_type & 0x80)
    {
        for (uint8_t function = 1; function < 8; function++)
        {
            pci_probe_function(bus, device, function);
        }
    }
}

/**
 * Varre recursivamente todos os 256 barramentos lógicos possíveis.
 */
void pci_bus_init(void)
{
    kprintf("[PCI] Iniciando varrimento completo do barramento (Mecanismo 1)...\n");

    /* Reseta os ponteiros lógicos e contadores do pool estático */
    g_pci_devices_head = NULL;
    g_pci_pool_count = 0;

    /* Varre todas as combinações estáveis de barramentos (0-255) e dispositivos/slots (0-31) */
    for (uint16_t bus = 0; bus < 256; bus++)
    {
        for (uint8_t device = 0; device < 32; device++)
        {
            pci_probe_device((uint8_t)bus, device);
        }
    }

    kprintf("[PCI] Varrimento concluido com sucesso. %d dispositivos em pool estatico.\n",
        g_pci_pool_count);
}

/**
 * Interface de acoplamento de drivers. Varre a lista ligada em busca de correspondências.
 */
int pci_register_driver(const char *name, uint16_t vendor_id, uint16_t device_id, int (*probe_cb)(pci_device_t *dev))
{
    if (!probe_cb)
        return -1;

    pci_device_t *current = g_pci_devices_head;
    int matches = 0;

    while (current)
    {
        if (current->vendor_id == vendor_id && current->device_id == device_id)
        {
            kprintf("[PCI] Acoplando driver '%s' ao hardware em %d:%d:%d\n",
                    name, current->address.bus, current->address.device, current->address.function);

            /* Chama a função de callback do driver passando o nó do dispositivo */
            if (probe_cb(current) == 0)
            {
                matches++;
            }
        }
        current = current->next;
    }

    return (matches > 0) ? 0 : -1;
}

/**
 * Procura um dispositivo específico na lista ligada através do Vendor ID e Device ID.
 * Útil para drivers que conhecem exatamente o chip de hardware alvo.
 * 
 * @return Ponteiro para a estrutura pci_device_t se encontrado, ou NULL.
 */
pci_device_t* pci_find_device(uint16_t vendor_id, uint16_t device_id)
{
    pci_device_t *current = g_pci_devices_head;

    while (current)
    {
        if (current->vendor_id == vendor_id && current->device_id == device_id)
        {
            return current; /* Retorna a referência direta do pool estático */
        }
        current = current->next;
    }

    return NULL;
}

/**
 * Procura um dispositivo através dos códigos de Classe e Subclasse.
 * 
 * @param from Dispositivo a partir do qual continuar a busca (use NULL para iniciar do topo).
 * @return Ponteiro para o próximo dispositivo correspondente encontrado, ou NULL.
 */
pci_device_t* pci_find_by_class_end_subclass(uint8_t class_code, uint8_t subclass_code, pci_device_t *from)
{
    pci_device_t *current = (from == NULL) ? g_pci_devices_head : from->next;

    while (current)
    {
        if (current->class_code == class_code && current->subclass_code == subclass_code)
        {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

/**
 * Procura um dispositivo através dos códigos de Classe.
 * 
 * @param from Dispositivo a partir do qual continuar a busca (use NULL para iniciar do topo).
 * @return Ponteiro para o próximo dispositivo correspondente encontrado, ou NULL.
 */
pci_device_t* pci_find_by_class(uint8_t class_code, pci_device_t *from)
{
    pci_device_t *current = (from == NULL) ? g_pci_devices_head : from->next;

    while (current)
    {
        if (current->class_code == class_code)
        {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

/**
 * Expõe a cabeça da lista ligada de dispositivos PCI.
 * Permite que loops externos enumerem recursivamente todo o hardware mapeado.
 */
pci_device_t* pci_get_devices_head(void)
{
    return g_pci_devices_head;
}


/**
 * Procura e carrega em loop todos os dispositivos que pertençam a uma mesma 
 * classe e subclasse. Resolve o cenário de máquinas com múltiplos controladores
 * USB (ex: xHCI e eHCI) ou múltiplos barramentos I2C.
 * 
 * @param class_code Código da classe PCI procurada.
 * @param subclass_code Código da subclasse PCI procurada.
 * @param init_cb Função callback que inicializa o driver específico deste hardware.
 * @return O número total de dispositivos carregados com sucesso.
 */
int pci_load_devices_by_class(uint8_t class_code, uint8_t subclass_code, int (*init_cb)(pci_device_t *dev))
{
    if (!init_cb) return -1;

    pci_device_t *current = NULL;
    int devices_loaded = 0;

    /* 
     * O loop utiliza a função pci_find_by_class passando o ponteiro 'current' anterior.
     * Isto faz com que a busca continue exatamente a partir do último nó encontrado.
     */
    while ((current = pci_find_by_class_end_subclass(class_code, subclass_code, current)) != NULL)
    {
        kprintf("[PCI] Carregando dispositivo de barramento em %02x:%02x.%d | Vid:0x%04x Did:0x%04x\n",
                current->address.bus, current->address.device, current->address.function,
                current->vendor_id, current->device_id);

        /* Executa a inicialização individual do controlador através do callback */
        if (init_cb(current) == 0)
        {
            devices_loaded++;
        }
    }

    return devices_loaded;
}


/**
 * Ativa os recursos fundamentais de barramento para um dispositivo PCI mapeado:
 * - Bus Mastering (Permite transações DMA nativas de alta velocidade)
 * - Memory Space MMIO (Habilita a resposta aos BARs de memória)
 */
void pci_enable_mmio_busmastering(pci_device_t *dev)
{
    if (!dev) return;

    /* Lê o registo de Comando PCI padrão (Offset 0x04) */
    uint32_t pci_cmd = pci_config_read_dword(dev->address.bus, dev->address.device, dev->address.function, PCI_REG_COMMAND);

    /* 
     * Injeta os bits cirúrgicos da especificação PCI/PCIe:
     * Bit 1: Memory Space Enable (0x02)
     * Bit 2: Bus Master Enable (0x04)
     * Bit 6: Parity Error Response Enable (0x40) - Necessário para sincronia PCIe no ICH9
     * Bit 8: SERR# Enable (0x100) - EXIGIDO pelo ICH9 para permitir o disparo de INTx legados
     */
    pci_cmd |= (1 << 1);  // Ativa Memory Space MMIO
    pci_cmd |= (1 << 2);  // Ativa Bus Mastering DMA
    pci_cmd |= (1 << 6);  // Ativa Resposta de Erro de Paridade (PCIe Host Sync)
    pci_cmd |= (1 << 8);  // Ativa SERR# (Obrigatório para encaminhar INTx no ICH9)
    
    /* Garante que o Bit 10 (Interrupt Disable) está a ZERO para ativar interrupções elétricas */
    pci_cmd &= ~(1 << 10); 

    /* Grava a nova palavra de controlo estável no espaço de configuração */
    pci_config_write_dword(dev->address.bus, dev->address.device, dev->address.function, PCI_REG_COMMAND, pci_cmd);
}


/*
 * Roteamento Universal de Interrupções PCI utilizando o subsistema ACPI.
 * Resolve o mistério do ICH9: Lê o IRQ 9 do PCI e traduz para GSI 19 via MADT.
 */
uint8_t pci_route_ioapic_irq(pci_device_t *dev) 
{
    // 1. Lê a linha padrão pré-configurada pelo firmware no registo 0x3C (Retorna 9 no ICH9)
    uint8_t pci_line = pci_config_read_byte(dev->address.bus, dev->address.device, dev->address.function, 0x3C);
  
    // Se o registo vier vazio (0xFF ou 0), calcula o pino elétrico genérico por swizzling
    if (pci_line == 0xFF || pci_line == 0)
    {
        uint8_t int_pin = pci_config_read_byte(dev->address.bus, dev->address.device, dev->address.function, 0x3D);
        pci_line = 16 + ((dev->address.device + (int_pin - 1)) % 4);
    }

    kprintf("[PCI] Linha lida do registo 0x3C: %d\n", pci_line);

    // 2. CONSULTA A MADT DO ACPI: 
    // No ICH9, isto vai detetar o Source Override: ISA 9 -> GSI 19 e devolverá 19!
    // No PIIX3, como não há override para a rede, devolverá o IRQ original (ex: 11) de forma 1:1.
    uint8_t final_gsi = (uint8_t)acpi_get_ioapic_irq(pci_line);

    kprintf("[PCI] Vetor final roteado via ACPI para o IOAPIC: %d\n", final_gsi);

    return final_gsi;
}
