#include "main.h"
#include <string.h>
#include "usart.h"
extern UART_HandleTypeDef huart1;

uint8_t rx_byte;
char rx_buffer[64];
uint8_t rx_index = 0;

void UART_Start_Receive(void)
{
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

void handle_command(char *cmd)
{
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
    if (strcmp(cmd, "LED_ON") == 0)
    {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
    }
    else if (strcmp(cmd, "LED_OFF") == 0)
    {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
    }
    else if (strcmp(cmd, "MOTOR_FORWARD") == 0)
    {
        // 开启电机正转，例如设置方向 GPIO + PWM
    }
    else if (strcmp(cmd, "MOTOR_STOP") == 0)
    {
        // 停止 PWM 或关闭电机驱动
    }
}


void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        if (rx_byte == '\n')
        {
            rx_buffer[rx_index] = '\0';
            handle_command(rx_buffer);
            rx_index = 0;
        }
        else
        {
            if (rx_index < sizeof(rx_buffer) - 1)
            {
                rx_buffer[rx_index++] = rx_byte;
            }
            else
            {
                rx_index = 0;
            }
        }

        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    }
}


