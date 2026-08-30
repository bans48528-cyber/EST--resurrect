#include <stddef.h>
#include <stdint.h>

#include "board_lcd.h"
#include "est_ui_text.h"

typedef struct {
	const char *chinese;
	const char *english;
	const char *portuguese;
} est_ui_text_entry_t;

static const est_ui_text_entry_t text_catalog[EST_UI_STRING_COUNT] = {
	[EST_UI_STRING_HOME_RECENT_EMPTY] = {
		"暂无最近程序", "No recent program", "Sem programa recente"
	},
	[EST_UI_STRING_HOME_RECENT_LABEL] = {"最近", "Recent", "Recente"},
	[EST_UI_STRING_HOME_PROGRAMS] = {"程序", "Prog.", "Prog."},
	[EST_UI_STRING_HOME_PORTS] = {"端口", "Ports", "Portas"},
	[EST_UI_STRING_HOME_REMOTE] = {"遥控", "Remote", "Remoto"},
	[EST_UI_STRING_HOME_MOTOR] = {"马达", "Motor", "Motor"},
	[EST_UI_STRING_HOME_SETTINGS] = {"设置", "Setup", "Config."},
	[EST_UI_STRING_POWER_TITLE] = {"确认关机？", "Power off?", "Desligar?"},
	[EST_UI_STRING_POWER_BODY] = {
		"是否关闭 EST？", "Turn off EST?", "Desligar o EST?"
	},
	[EST_UI_STRING_POWER_CANCEL_HINT] = {
		"返回 取消", "Back: Cancel", "Voltar: Cancelar"
	},
	[EST_UI_STRING_POWER_CONFIRM_HINT] = {
		"确认 关机", "Confirm: Off", "Confirmar: Desligar"
	},
	[EST_UI_STRING_PROGRAMS_TITLE] = {"程序", "Programs", "Programas"},
	[EST_UI_STRING_PROGRAMS_EMPTY] = {
		"没有程序", "No programs", "Sem programas"
	},
	[EST_UI_STRING_PROGRAMS_LOADING] = {
		"正在读取", "Loading", "Carregando"
	},
	[EST_UI_STRING_DELETE_TITLE] = {
		"删除程序？", "Delete program?", "Excluir programa?"
	},
	[EST_UI_STRING_DELETE_IRREVERSIBLE] = {
		"操作无法恢复", "Cannot be undone", "Não pode ser desfeito"
	},
	[EST_UI_STRING_CANCEL] = {"取消", "Cancel", "Cancelar"},
	[EST_UI_STRING_DELETE] = {"删除", "Delete", "Excluir"},
	[EST_UI_STRING_DELETE_HINT] = {">删除", ">Del", ">Excluir"},
	[EST_UI_STRING_RUNNING] = {"运行中", "RUNNING", "EXECUTANDO"},
	[EST_UI_STRING_PORTS_TITLE] = {"端口查看", "Ports", "Portas"},
	[EST_UI_STRING_MOTORS] = {"马达", "Motors", "Motores"},
	[EST_UI_STRING_SENSORS] = {"传感器", "Sensors", "Sensores"},
	[EST_UI_STRING_DEVICE] = {"设备", "Device", "Dispositivo"},
	[EST_UI_STRING_STATE] = {"状态", "State", "Estado"},
	[EST_UI_STRING_VALUE] = {"数值", "Value", "Valor"},
	[EST_UI_STRING_DISCONNECTED] = {
		"未连接", "Disconnected", "Desconectado"
	},
	[EST_UI_STRING_DETECTING] = {"识别中", "Detecting", "Detectando"},
	[EST_UI_STRING_STOPPED] = {"停止", "Stopped", "Parado"},
	[EST_UI_STRING_ACTIVE] = {"运行", "Running", "Ativo"},
	[EST_UI_STRING_BRAKING] = {"制动", "Brake", "Freio"},
	[EST_UI_STRING_LARGE_MOTOR] = {
		"大型马达", "Large motor", "Motor grande"
	},
	[EST_UI_STRING_MEDIUM_MOTOR] = {
		"中型马达", "Medium motor", "Motor médio"
	},
	[EST_UI_STRING_SETTINGS_TITLE] = {"设置", "Settings", "Configurações"},
	[EST_UI_STRING_BACKLIGHT] = {"背光", "Backlight", "Luz de fundo"},
	[EST_UI_STRING_VOLUME] = {"音量", "Volume", "Volume"},
	[EST_UI_STRING_LANGUAGE] = {"语言", "Language", "Idioma"},
	[EST_UI_STRING_CHINESE] = {"中文", "Chinese", "Chinês"},
	[EST_UI_STRING_PORTUGUESE] = {"Português BR", "Portuguese BR", "Português BR"},
	[EST_UI_STRING_DEVICE_INFO] = {
		"设备信息", "Device Info", "Info. do disp."
	},
	[EST_UI_STRING_MODEL] = {"型号", "Model", "Modelo"},
	[EST_UI_STRING_FIRMWARE] = {"固件", "Firmware", "Firmware"},
	[EST_UI_STRING_BOOTLOADER] = {"引导", "Bootloader", "Bootloader"},
	[EST_UI_STRING_USB] = {"USB", "USB", "USB"},
	[EST_UI_STRING_BATTERY] = {"电量", "Battery", "Bateria"},
	[EST_UI_STRING_CONNECTED] = {"已连接", "Connected", "Conectado"},
	[EST_UI_STRING_LOW_BATTERY] = {
		"低电量", "Low battery", "Bateria fraca"
	},
	[EST_UI_STRING_STORAGE_ERROR] = {
		"存储错误", "Storage error", "Erro de memória"
	},
	[EST_UI_STRING_PROGRAM_LIST_UNREADABLE] = {
		"程序列表无法读取", "Cannot read program list",
		"Falha ao ler programas"
	},
	[EST_UI_STRING_PROGRAM_UNREADABLE] = {
		"程序无法读取", "Cannot read program", "Falha ao ler programa"
	},
	[EST_UI_STRING_PROGRAM_STOPPED] = {
		"程序已停止", "Program stopped", "Programa parado"
	},
	[EST_UI_STRING_MOTORS_SAFE] = {
		"马达已安全停止", "Motors safely stopped", "Motores parados"
	},
	[EST_UI_STRING_CHARGE_BEFORE_RUN] = {
		"充电后再运行程序", "Charge before running",
		"Carregue antes de executar"
	},
	[EST_UI_STRING_RETRY] = {
		"确认 重试", "Confirm: Retry", "Confirmar: Tentar de novo"
	},
	[EST_UI_STRING_REMOTE_TITLE] = {"红外遥控", "Remote", "Controle IR"},
	[EST_UI_STRING_REMOTE_CONNECT_IR] = {
		"请连接红外到端口4", "Connect IR to 4", "Conecte IR à porta 4"
	},
	[EST_UI_STRING_REMOTE_SIGNAL_LOST] = {
		"信号丢失", "Signal lost", "Sinal perdido"
	},
	[EST_UI_STRING_REMOTE_DEVICE_LOST] = {
		"设备未连接", "Device disconnected", "Dispositivo desconectado"
	},
	[EST_UI_STRING_REMOTE_BRAKE_ALL] = {
		"A-D 制动", "A-D BRAKE", "A-D FREIO"
	},
	[EST_UI_STRING_MOTOR_OUTPUT_TITLE] = {
		"端口输出", "Motor Out", "Saída motor"
	},
	[EST_UI_STRING_MOTOR_STOP] = {"停止", "Stop", "Parar"},
	[EST_UI_STRING_MOTOR_FORWARD] = {"正转", "Fwd", "Fte"},
	[EST_UI_STRING_MOTOR_REVERSE] = {"反转", "Rev", "Ré"},
	[EST_UI_STRING_MOTOR_POWER] = {"功率", "Power", "Potência"},
	[EST_UI_STRING_RECEIVING_PROGRAM] = {
		"程序传输中", "Receiving program", "Recebendo prog."
	},
	[EST_UI_STRING_POWER_OFF_SHORT] = {"关机", "Off", "Desligar"},
};

static bool language_valid(est_ui_language_t language)
{
	return language == EST_UI_LANGUAGE_CHINESE ||
		language == EST_UI_LANGUAGE_ENGLISH ||
		language == EST_UI_LANGUAGE_PORTUGUESE;
}

const char *est_ui_text(est_ui_language_t language,
	est_ui_string_id_t string_id)
{
	if (!language_valid(language) || string_id >= EST_UI_STRING_COUNT) {
		return NULL;
	}
	if (language == EST_UI_LANGUAGE_CHINESE) {
		return text_catalog[string_id].chinese;
	}
	if (language == EST_UI_LANGUAGE_PORTUGUESE) {
		return text_catalog[string_id].portuguese;
	}
	return text_catalog[string_id].english;
}

uint16_t est_ui_text_width(est_ui_language_t language,
	est_ui_string_id_t string_id, est_ui_text_style_t style)
{
	return est_ui_font_measure(est_ui_text(language, string_id), style);
}

est_result_t est_ui_text_draw(uint16_t x, uint16_t y,
	est_ui_language_t language, est_ui_string_id_t string_id,
	est_ui_text_style_t style)
{
	const char *text = est_ui_text(language, string_id);

	return text != NULL && board_lcd_draw_ui_text(x, y, text, style) ?
		EST_OK : EST_ERR_INVALID_ARGUMENT;
}

est_result_t est_ui_text_draw_raw(uint16_t x, uint16_t y,
	const char *text, est_ui_text_style_t style)
{
	return board_lcd_draw_ui_text(x, y, text, style) ? EST_OK :
		EST_ERR_INVALID_ARGUMENT;
}
