/*****************************************************************************
 * | Fichier    :   main.c
 * | Auteur     :   équipe Waveshare
 * | Rôle       :   Fonction principale
 * | Description:   Lit des fichiers BMP depuis la carte SD et les affiche à l'écran.
 * |                Utiliser l'écran tactile pour passer d'une image à l'autre.
 *----------------
 * | Version    :   V1.0
 * | Date       :   2024-12-06
 * | Note       :   Version de base + sélection de dossier (Reptiles/Presentation)
 *
 ******************************************************************************/

#include "rgb_lcd_port.h"    // En-tête du pilote LCD RGB Waveshare
#include "gui_paint.h"       // En-tête des fonctions de dessin graphique
#include "gui_bmp.h"         // En-tête pour la gestion des images BMP
#include "gt911.h"           // En-tête des opérations de l'écran tactile (GT911)
#include "sd.h"              // En-tête des opérations sur carte SD

#include <dirent.h>          // En-tête pour les opérations sur répertoires
#include <stdio.h>

static char *BmpPath[256];        // Tableau pour stocker les chemins des fichiers BMP
static uint8_t bmp_num;           // Nombre de fichiers BMP trouvés

// Fonction listant tous les fichiers BMP d'un répertoire
void list_files(const char *base_path) {
    int i = 0;
    DIR *dir = opendir(base_path);  // Ouvre le répertoire
    if (dir) {
        struct dirent *entry;
        // Parcourt tous les fichiers du répertoire
        while ((entry = readdir(dir)) != NULL) {
            const char *file_name = entry->d_name;
            // Vérifie si le fichier se termine par ".bmp"
            size_t len = strlen(file_name);
            if (len > 4 && strcasecmp(&file_name[len - 4], ".bmp") == 0) {
                if (i >= 256) {
                    printf("Erreur : quota maximal de chemins BMP atteint (256)\n");
                    break;
                }
                size_t length = strlen(base_path) + strlen(file_name) + 2; // 1 pour '/' et 1 pour '\0'
                BmpPath[i] = malloc(length);  // Alloue la mémoire pour le chemin du BMP
                snprintf(BmpPath[i], length, "%s/%s", base_path, file_name); // Enregistre le chemin complet
                i++;
            }
        }
        bmp_num = i; // Met à jour le nombre de fichiers BMP trouvés
        closedir(dir); // Ferme le répertoire
    }
}

void free_bmp_paths(void)
{
    for (int i = 0; i < bmp_num && i < 256; i++) {
        free(BmpPath[i]);
        BmpPath[i] = NULL;
    }
    bmp_num = 0;
}

// Fonction principale de l'application
void app_main()
{
    touch_gt911_point_t point_data;  // Structure pour stocker les données du point tactile

    // Initialise le contrôleur tactile GT911
    touch_gt911_init();  
    
    // Initialise le matériel LCD RGB du Waveshare ESP32-S3
    waveshare_esp32_s3_rgb_lcd_init(); 

    // Allume le rétroéclairage du LCD
    wavesahre_rgb_lcd_bl_on();         

    // EXAMPLE_PIN_NUM_TOUCH_INT
    // Alloue la mémoire pour le tampon d'image (framebuffer) du LCD
    UDOUBLE Imagesize = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * 2; // Chaque pixel utilise 2 octets (RGB565)
    UBYTE *BlackImage;
    if ((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) // Vérifie si l'allocation mémoire a réussi
    {
        printf("Échec d’allocation mémoire pour le framebuffer...\r\n");
        exit(0); // Quitte en cas d'échec d'allocation mémoire
    }

    // Initialise la toile graphique avec le tampon alloué
    Paint_NewImage(BlackImage, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, 0, WHITE);

    // Définit l’échelle pour la toile graphique
    Paint_SetScale(65);

    // Efface la toile et la remplit en blanc
    Paint_Clear(WHITE);

    // Initialise la carte SD
    if (sd_mmc_init() == ESP_OK) 
    {
        /************* DÉBUT : ÉCRAN DE SÉLECTION DU DOSSIER *************/
        // Message d'état
        Paint_DrawString_EN(200, 200, "Carte SD OK !", &Font24, BLACK, WHITE);
        Paint_DrawString_EN(200, 240, "Choisissez un dossier :", &Font24, BLACK, WHITE);

        // Définition des deux zones "boutons" (rectangles tactiles)
        // Bouton gauche : Reptiles
        UWORD btnL_x0 = 120;
        UWORD btnL_y0 = 280;
        UWORD btnL_x1 = 440;   // largeur 320
        UWORD btnL_y1 = 400;   // hauteur 120

        // Bouton droit : Presentation (positionné depuis la droite de l'écran)
        UWORD btnR_x1 = EXAMPLE_LCD_H_RES - 120;
        UWORD btnR_x0 = btnR_x1 - 320;
        UWORD btnR_y0 = 280;
        UWORD btnR_y1 = 400;

        // Dessine les rectangles (au trait) et les libellés
        // Rectangle gauche (Reptiles)
        Paint_DrawLine(btnL_x0, btnL_y0, btnL_x1, btnL_y0, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
        Paint_DrawLine(btnL_x1, btnL_y0, btnL_x1, btnL_y1, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
        Paint_DrawLine(btnL_x1, btnL_y1, btnL_x0, btnL_y1, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
        Paint_DrawLine(btnL_x0, btnL_y1, btnL_x0, btnL_y0, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
        Paint_DrawString_EN(btnL_x0 + 60, (btnL_y0 + btnL_y1)/2 - 12, "Reptiles", &Font24, BLACK, WHITE);

        // Rectangle droit (Presentation)
        Paint_DrawLine(btnR_x0, btnR_y0, btnR_x1, btnR_y0, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
        Paint_DrawLine(btnR_x1, btnR_y0, btnR_x1, btnR_y1, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
        Paint_DrawLine(btnR_x1, btnR_y1, btnR_x0, btnR_y1, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
        Paint_DrawLine(btnR_x0, btnR_y1, btnR_x0, btnR_y0, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
        Paint_DrawString_EN(btnR_x0 + 40, (btnR_y0 + btnR_y1)/2 - 12, "Presentation", &Font24, BLACK, WHITE);

        // Affiche l'écran de sélection
        wavesahre_rgb_lcd_display(BlackImage);

        // Attente de la sélection tactile
        const char *selected_dir = NULL;
        while (selected_dir == NULL) {
            point_data = touch_gt911_read_point(1);
            if (point_data.cnt == 1) {
                uint16_t tx = point_data.x[0];
                uint16_t ty = point_data.y[0];

                // Test zone "Reptiles"
                if (tx >= btnL_x0 && tx <= btnL_x1 && ty >= btnL_y0 && ty <= btnL_y1) {
                    selected_dir = "Reptiles";
                }
                // Test zone "Presentation"
                else if (tx >= btnR_x0 && tx <= btnR_x1 && ty >= btnR_y0 && ty <= btnR_y1) {
                    selected_dir = "Presentation";
                }
            }
            vTaskDelay(30);
        }

        // Construit le chemin de base en fonction du dossier choisi
        char base_path[128];
        snprintf(base_path, sizeof(base_path), "%s/%s", MOUNT_POINT, selected_dir);

        // Nettoie l'écran et affiche un rappel
        Paint_Clear(WHITE);
        Paint_DrawString_EN(200, 200, "Dossier choisi :", &Font24, BLACK, WHITE);
        Paint_DrawString_EN(200, 240, (char *)selected_dir, &Font24, BLACK, WHITE);
        Paint_DrawString_EN(200, 280, "Touchez la fleche pour demarrer.", &Font24, BLACK, WHITE);
        wavesahre_rgb_lcd_display(BlackImage);

        // Liste les BMP du dossier sélectionné
        list_files(base_path);
        if (bmp_num == 0)
        {
            Paint_DrawString_EN(200, 320, "Aucun fichier BMP dans ce dossier.", &Font24, RED, WHITE);
            wavesahre_rgb_lcd_display(BlackImage);
            free_bmp_paths();
            return;
        }
        else
        {
            // Dessine les flèches de navigation (identiques à l'implémentation d’origine)
            Paint_DrawLine(EXAMPLE_LCD_H_RES-80, 20, EXAMPLE_LCD_H_RES-20, 20, RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
            Paint_DrawLine(EXAMPLE_LCD_H_RES-20, 20, EXAMPLE_LCD_H_RES-45, 5,  RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID); // Flèche droite
            Paint_DrawLine(EXAMPLE_LCD_H_RES-20, 20, EXAMPLE_LCD_H_RES-45, 35, RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID); 

            // Affiche l'écran initial
            wavesahre_rgb_lcd_display(BlackImage);
        }
        /************* FIN : ÉCRAN DE SÉLECTION DU DOSSIER *************/
        
    }
    else
    {
        // Si l'initialisation de la carte SD échoue
        Paint_DrawString_EN(200, 200, "Échec carte SD !", &Font24, BLACK, WHITE);
        wavesahre_rgb_lcd_display(BlackImage);
        free_bmp_paths();
        return;

    }

    // Variables initiales du point tactile
    int8_t i = 0;
    static uint16_t prev_x;
    static uint16_t prev_y;

    while (1)
    {
        point_data = touch_gt911_read_point(1);  // Lit les données tactiles
        if (point_data.cnt == 1)  // Vérifie si un contact est détecté
        {
            // Si la position n’a pas changé, continue la boucle
            if ((prev_x == point_data.x[0]) && (prev_y == point_data.y[0]))
            {
                continue;
            }
            
            // Si le toucher est dans la zone de navigation gauche, image précédente
            if (point_data.x[0] >= 20 && point_data.x[0] <= 80 && point_data.y[0] >= 0 && point_data.y[0] <= 60)
            {
                i--;
                if (i < 0)  // Si l’index passe sous 0, revient à la dernière image
                {
                    i = bmp_num - 1;
                }
                Paint_Clear(WHITE);  // Efface l’écran
                GUI_ReadBmp(0, 0, BmpPath[i]);  // Lit et affiche l'image BMP précédente

                // Flèche gauche
                Paint_DrawLine(20, 20, 80, 20, RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
                Paint_DrawLine(20, 20, 45, 5,  RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
                Paint_DrawLine(20, 20, 45, 35, RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);

                // Flèche droite
                Paint_DrawLine(EXAMPLE_LCD_H_RES-80, 20, EXAMPLE_LCD_H_RES-20, 20, RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
                Paint_DrawLine(EXAMPLE_LCD_H_RES-20, 20, EXAMPLE_LCD_H_RES-45, 5,  RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
                Paint_DrawLine(EXAMPLE_LCD_H_RES-20, 20, EXAMPLE_LCD_H_RES-45, 35, RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);

                wavesahre_rgb_lcd_display(BlackImage);  // Met à jour l’affichage
                
                prev_x = point_data.x[0];  // Met à jour la position tactile précédente
                prev_y = point_data.y[0];
            }
            // Si le toucher est dans la zone de navigation droite, image suivante
            else if (point_data.x[0] >= EXAMPLE_LCD_H_RES-80 && point_data.x[0] <= EXAMPLE_LCD_H_RES-20 && point_data.y[0] >= 0 && point_data.y[0] <= 60)
            {
                i++;
                if (i > bmp_num - 1)  // Si l’index dépasse le nombre d’images, revient à la première image
                {
                    i = 0;
                }
                Paint_Clear(WHITE);  // Efface l’écran
                GUI_ReadBmp(0, 0, BmpPath[i]);  // Lit et affiche l'image BMP suivante

                // Flèche gauche
                Paint_DrawLine(20, 20, 80, 20, RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
                Paint_DrawLine(20, 20, 45, 5,  RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
                Paint_DrawLine(20, 20, 45, 35, RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);

                // Flèche droite
                Paint_DrawLine(EXAMPLE_LCD_H_RES-80, 20, EXAMPLE_LCD_H_RES-20, 20, RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
                Paint_DrawLine(EXAMPLE_LCD_H_RES-20, 20, EXAMPLE_LCD_H_RES-45, 5,  RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
                Paint_DrawLine(EXAMPLE_LCD_H_RES-20, 20, EXAMPLE_LCD_H_RES-45, 35, RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);

                wavesahre_rgb_lcd_display(BlackImage);  // Met à jour l’affichage

                prev_x = point_data.x[0];  // Met à jour la position tactile précédente
                prev_y = point_data.y[0];
            }          
        }
        vTaskDelay(30);  // Délai de 30 ms pour éviter une utilisation CPU excessive
    }
}
