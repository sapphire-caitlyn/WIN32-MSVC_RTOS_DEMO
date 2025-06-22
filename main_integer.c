/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

 /******************************************************************************
  * Complete integer math demo - equivalent to main_blinky but for integer tasks
  *
  * This file contains:
  * 1. All the original integer math task code from main_integer.c
  * 2. A main_integer() function equivalent to main_blinky()
  * 3. Monitoring and user interaction capabilities
  *******************************************************************************/

  /* Standard includes. */
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

/* The constants used in the calculation. */
#define intgCONST1             ( ( long ) 123 )
#define intgCONST2             ( ( long ) 234567 )
#define intgCONST3             ( ( long ) -3 )
#define intgCONST4             ( ( long ) 7 )
#define intgEXPECTED_ANSWER    ( ( ( intgCONST1 + intgCONST2 ) * intgCONST3 ) / intgCONST4 )

#define intgSTACK_SIZE         configMINIMAL_STACK_SIZE

/* As this is the minimal version, we will only create one task. */
#define intgNUMBER_OF_TASKS    ( 1 )

/* Priorities at which the tasks are created. */
#define mainMONITOR_TASK_PRIORITY          ( tskIDLE_PRIORITY + 2 )
#define mainINTEGER_TASK_PRIORITY          ( tskIDLE_PRIORITY + 1 )

/* The rate at which the monitor task checks the integer math tasks. */
#define mainMONITOR_FREQUENCY_MS           pdMS_TO_TICKS( 2000UL )

/* This demo allows for users to perform actions with the keyboard. */
#define mainNO_KEY_PRESS_VALUE             ( -1 )
#define mainSTATUS_KEY                     ( 's' )
#define mainRESTART_KEY                    ( 'r' )

/*-----------------------------------------------------------*/

/* Function prototypes from original main_integer.c */
static portTASK_FUNCTION_PROTO(vCompeteingIntMathTask, pvParameters);
void vStartIntegerMathTasks(UBaseType_t uxPriority);
BaseType_t xAreIntegerMathsTaskStillRunning(void);

/* New function prototypes for main_integer demo */
static void prvMonitorTask(void* pvParameters);
static void prvMonitorTimerCallback(TimerHandle_t xTimerHandle);

/*-----------------------------------------------------------*/

/* Variables that are set to true within the calculation task to indicate
 * that the task is still executing.  The check task sets the variable back to
 * false, flagging an error if the variable is still false the next time it
 * is called. */
static BaseType_t xTaskCheck[intgNUMBER_OF_TASKS] = { (BaseType_t)pdFALSE };

/* A software timer that triggers periodic status checks. */
static TimerHandle_t xMonitorTimer = NULL;

/* Flag to indicate if a status check should be performed. */
static BaseType_t xPerformStatusCheck = pdFALSE;

/*-----------------------------------------------------------*/

/*** ORIGINAL INTEGER MATH TASK CODE FROM main_integer.c ***/

void vStartIntegerMathTasks(UBaseType_t uxPriority)
{
    short sTask;

    for (sTask = 0; sTask < intgNUMBER_OF_TASKS; sTask++)
    {
        xTaskCreate(vCompeteingIntMathTask, "IntMath", intgSTACK_SIZE, (void*)&(xTaskCheck[sTask]), uxPriority, (TaskHandle_t*)NULL);
    }
}
/*-----------------------------------------------------------*/

static portTASK_FUNCTION(vCompeteingIntMathTask, pvParameters)
{
    /* These variables are all effectively set to constants so they are volatile to
     * ensure the compiler does not just get rid of them. */
    volatile long lValue;
    short sError = pdFALSE;
    volatile BaseType_t* pxTaskHasExecuted;

    /* Set a pointer to the variable we are going to set to true each
     * iteration.  This is also a good test of the parameter passing mechanism
     * within each port. */
    pxTaskHasExecuted = (volatile BaseType_t*)pvParameters;

    /* Keep performing a calculation and checking the result against a constant. */
    for (; ; )
    {
        /* Perform the calculation.  This will store partial value in
         * registers, resulting in a good test of the context switch mechanism. */
        lValue = intgCONST1;
        lValue += intgCONST2;

        /* Yield in case cooperative scheduling is being used. */
#if configUSE_PREEMPTION == 0
        {
            taskYIELD();
        }
#endif

        /* Finish off the calculation. */
        lValue *= intgCONST3;
        lValue /= intgCONST4;

        /* If the calculation is found to be incorrect we stop setting the
         * TaskHasExecuted variable so the check task can see an error has
         * occurred. */
        if (lValue != intgEXPECTED_ANSWER) /*lint !e774 volatile used to prevent this being optimised out. */
        {
            sError = pdTRUE;
        }

        if (sError == pdFALSE)
        {
            /* We have not encountered any errors, so set the flag that show
             * we are still executing.  This will be periodically cleared by
             * the check task. */
            portENTER_CRITICAL();
            *pxTaskHasExecuted = pdTRUE;
            portEXIT_CRITICAL();
        }

        /* Yield in case cooperative scheduling is being used. */
#if configUSE_PREEMPTION == 0
        {
            taskYIELD();
        }
#endif
    }
}
/*-----------------------------------------------------------*/

/* This is called to check that all the created tasks are still running. */
BaseType_t xAreIntegerMathsTaskStillRunning(void)
{
    BaseType_t xReturn = pdTRUE;
    short sTask;

    /* Check the maths tasks are still running by ensuring their check variables
     * are still being set to true. */
    for (sTask = 0; sTask < intgNUMBER_OF_TASKS; sTask++)
    {
        if (xTaskCheck[sTask] == pdFALSE)
        {
            /* The check has not incremented so an error exists. */
            xReturn = pdFALSE;
        }

        /* Reset the check variable so we can tell if it has been set by
         * the next time around. */
        xTaskCheck[sTask] = pdFALSE;
    }

    return xReturn;
}

/*-----------------------------------------------------------*/

/*** NEW MAIN_INTEGER DEMO CODE (EQUIVALENT TO main_blinky) ***/

/*** SEE THE COMMENTS AT THE TOP OF THIS FILE ***/
void main_integer(void)
{
    const TickType_t xTimerPeriod = mainMONITOR_FREQUENCY_MS;

    printf("\r\nStarting the integer math demo. Press '%c' to display status, '%c' to restart tasks.\r\n\r\n",
        mainSTATUS_KEY, mainRESTART_KEY);

    /* Start the integer math tasks. */
    vStartIntegerMathTasks(mainINTEGER_TASK_PRIORITY);

    /* Create the monitor task. */
    xTaskCreate(prvMonitorTask,                    /* The function that implements the task. */
        "Monitor",                         /* The text name assigned to the task. */
        configMINIMAL_STACK_SIZE,          /* The size of the stack to allocate to the task. */
        NULL,                              /* The parameter passed to the task. */
        mainMONITOR_TASK_PRIORITY,         /* The priority assigned to the task. */
        NULL);                            /* The task handle is not required. */

    /* Create the monitor timer, but don't start it yet. */
    xMonitorTimer = xTimerCreate("MonitorTimer",           /* Timer name. */
        xTimerPeriod,             /* Timer period. */
        pdTRUE,                   /* Auto-reload timer. */
        NULL,                     /* Timer ID not used. */
        prvMonitorTimerCallback);/* Timer callback function. */

    if (xMonitorTimer != NULL)
    {
        xTimerStart(xMonitorTimer, 0);                    /* The scheduler has not started so use a block time of 0. */
    }

    /* Start the tasks and timer running. */
    vTaskStartScheduler();

    /* If all is well, the scheduler will now be running, and the following
     * line will never be reached.  If the following line does execute, then
     * there was insufficient FreeRTOS heap memory available for the idle and/or
     * timer tasks to be created.  See the memory management section on the
     * FreeRTOS web site for more details. */
    for (; ; )
    {
    }
}
/*-----------------------------------------------------------*/

static void prvMonitorTask(void* pvParameters)
{
    BaseType_t xTasksStatus;
    static uint32_t ulStatusCheckCount = 0;

    /* Prevent the compiler warning about the unused parameter. */
    (void)pvParameters;

    for (; ; )
    {
        /* Wait for a status check request. */
        while (xPerformStatusCheck == pdFALSE)
        {
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        /* Reset the status check flag. */
        xPerformStatusCheck = pdFALSE;

        /* Check if the integer math tasks are still running correctly. */
        xTasksStatus = xAreIntegerMathsTaskStillRunning();

        /* Enter critical section for console output. */
        taskENTER_CRITICAL();
        {
            ulStatusCheckCount++;

            if (xTasksStatus == pdTRUE)
            {
                printf("Message received from integer task - Status check #%lu: PASS\r\n",
                    ulStatusCheckCount);
            }
            else
            {
                printf("Message received from monitor timer - Status check #%lu: FAIL - Error detected!\r\n",
                    ulStatusCheckCount);
            }
        }
        taskEXIT_CRITICAL();
    }
}
/*-----------------------------------------------------------*/

static void prvMonitorTimerCallback(TimerHandle_t xTimerHandle)
{
    /* Avoid compiler warnings resulting from the unused parameter. */
    (void)xTimerHandle;

    /* Set flag to trigger a status check - causing the monitor task to unblock and
     * write to the console. This function is called from the timer/daemon task, so
     * must not block. Hence the flag is used instead of direct console output. */
    xPerformStatusCheck = pdTRUE;
}
/*-----------------------------------------------------------*/

/* Called from prvKeyboardInterruptSimulatorTask(), which is defined in main.c. */
void vIntegerKeyboardInterruptHandler(int xKeyPressed)
{
    /* Handle keyboard input. */
    switch (xKeyPressed)
    {
    case mainSTATUS_KEY:
        /* Trigger an immediate status check. */
        taskENTER_CRITICAL();
        {
            printf("\r\nManual status check requested...\r\n");
        }
        taskEXIT_CRITICAL();

        xPerformStatusCheck = pdTRUE;
        break;

    case mainRESTART_KEY:
        taskENTER_CRITICAL();
        {
            printf("\r\nRestarting integer math tasks...\r\n");
            printf("Note: Task restart requires system reset in this demo.\r\n\r\n");
        }
        taskEXIT_CRITICAL();
        break;

    default:
        break;
    }
}