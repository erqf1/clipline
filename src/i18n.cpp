#include "i18n.h"

#include <QHash>
#include <QLocale>

// Sprachen wie in Cutline. Schlüssel ist der englische Text; Spalten: de es fr it pt nl pl tr ru ja zh
namespace {
constexpr int N = 11;
const char* kCodes[] = {"en", "de", "es", "fr", "it", "pt", "nl", "pl", "tr", "ru", "ja", "zh"};
const char* kNames[] = {"English", "Deutsch", "Español", "Français", "Italiano", "Português",
                        "Nederlands", "Polski", "Türkçe", "Русский", "日本語", "中文"};

struct Entry { const char* en; const char* t[N]; };

const Entry kTable[] = {
 {"%1 clips", {"%1 Clips","%1 clips","%1 clips","%1 clip","%1 clipes","%1 clips","%1 klipów","%1 klip","Клипов: %1","%1 件のクリップ","%1 个剪辑"}},
 {"1 clip", {"1 Clip","1 clip","1 clip","1 clip","1 clipe","1 clip","1 klip","1 klip","1 клип","1 件のクリップ","1 个剪辑"}},
 {"Accent colour", {"Akzentfarbe","Color de acento","Couleur d'accent","Colore di accento","Cor de destaque","Accentkleur","Kolor akcentu","Vurgu rengi","Цвет акцента","アクセントカラー","强调色"}},
 {"Back", {"Zurück","Atrás","Retour","Indietro","Voltar","Terug","Wstecz","Geri","Назад","戻る","返回"}},
 {"Balanced", {"Ausgewogen","Equilibrada","Équilibrée","Bilanciata","Equilibrada","Gebalanceerd","Zrównoważona","Dengeli","Баланс","標準","均衡"}},
 {"Cancel", {"Abbrechen","Cancelar","Annuler","Annulla","Cancelar","Annuleren","Anuluj","İptal","Отмена","キャンセル","取消"}},
 {"Choose your language", {"Sprache wählen","Elige tu idioma","Choisissez votre langue","Scegli la lingua","Escolha o idioma","Kies je taal","Wybierz język","Dilinizi seçin","Выберите язык","言語を選択","选择语言"}},
 {"Choose…", {"Auswählen…","Elegir…","Choisir…","Scegli…","Escolher…","Kiezen…","Wybierz…","Seç…","Выбрать…","選択…","选择…"}},
 {"Click the button and press the combination you want. Works everywhere, even in games.",
  {"Klick auf den Knopf und drück die gewünschte Kombination. Funktioniert überall, auch in Spielen.",
   "Pulsa el botón y la combinación que quieras. Funciona en todas partes, incluso en juegos.",
   "Cliquez sur le bouton puis appuyez sur la combinaison voulue. Fonctionne partout, même en jeu.",
   "Fai clic sul pulsante e premi la combinazione che vuoi. Funziona ovunque, anche nei giochi.",
   "Clique no botão e prima a combinação que quiser. Funciona em todo o lado, até em jogos.",
   "Klik op de knop en druk de gewenste combinatie in. Werkt overal, ook in games.",
   "Kliknij przycisk i naciśnij wybraną kombinację. Działa wszędzie, nawet w grach.",
   "Düğmeye tıklayın ve istediğiniz tuş kombinasyonuna basın. Her yerde, oyunlarda bile çalışır.",
   "Нажмите кнопку и нужное сочетание клавиш. Работает везде, даже в играх.",
   "ボタンをクリックして、使いたいキーの組み合わせを押してください。ゲーム中でもどこでも使えます。",
   "点击按钮并按下想要的组合键。在任何地方都有效，包括游戏中。"}},
 {"Clip length & quality", {"Cliplänge & Qualität","Duración y calidad","Durée et qualité","Durata e qualità","Duração e qualidade","Cliplengte & kwaliteit","Długość i jakość","Klip süresi ve kalite","Длина и качество","長さと画質","时长与画质"}},
 {"Clip length", {"Cliplänge","Duración del clip","Durée du clip","Durata della clip","Duração do clipe","Cliplengte","Długość klipu","Klip süresi","Длина клипа","クリップの長さ","剪辑时长"}},
 {"Clip saved", {"Clip gespeichert","Clip guardado","Clip enregistré","Clip salvata","Clipe guardado","Clip opgeslagen","Klip zapisany","Klip kaydedildi","Клип сохранён","クリップを保存しました","剪辑已保存"}},
 {"Clipline is still running in the background. Press %1 to save a clip.",
  {"Clipline läuft weiter im Hintergrund. Drücke %1, um einen Clip zu speichern.",
   "Clipline sigue funcionando en segundo plano. Pulsa %1 para guardar un clip.",
   "Clipline continue en arrière-plan. Appuyez sur %1 pour enregistrer un clip.",
   "Clipline è ancora attivo in background. Premi %1 per salvare una clip.",
   "O Clipline continua a funcionar em segundo plano. Prima %1 para guardar um clipe.",
   "Clipline draait nog op de achtergrond. Druk op %1 om een clip op te slaan.",
   "Clipline nadal działa w tle. Naciśnij %1, aby zapisać klip.",
   "Clipline arka planda çalışmaya devam ediyor. Klip kaydetmek için %1 tuşuna basın.",
   "Clipline продолжает работать в фоне. Нажмите %1, чтобы сохранить клип.",
   "Clipline はバックグラウンドで動作中です。%1 を押すとクリップを保存します。",
   "Clipline 仍在后台运行。按 %1 保存剪辑。"}},
 {"Clipline records your screen quietly in the background. Press a key and the last moments are saved as a clip – nothing is written to disk until you do.",
  {"Clipline nimmt deinen Bildschirm leise im Hintergrund auf. Ein Tastendruck, und die letzten Momente werden als Clip gespeichert – vorher wird nichts auf die Festplatte geschrieben.",
   "Clipline graba tu pantalla en silencio en segundo plano. Pulsa una tecla y los últimos momentos se guardan como clip; hasta entonces no se escribe nada en el disco.",
   "Clipline enregistre votre écran discrètement en arrière-plan. Appuyez sur une touche et les derniers instants sont enregistrés en clip – rien n'est écrit sur le disque avant.",
   "Clipline registra lo schermo in silenzio in background. Premi un tasto e gli ultimi momenti vengono salvati come clip: prima non viene scritto nulla sul disco.",
   "O Clipline grava o seu ecrã discretamente em segundo plano. Prima uma tecla e os últimos momentos são guardados como clipe – nada é escrito no disco antes disso.",
   "Clipline neemt je scherm stil op de achtergrond op. Druk op een toets en de laatste momenten worden als clip opgeslagen – daarvoor wordt niets naar de schijf geschreven.",
   "Clipline po cichu nagrywa ekran w tle. Naciśnij klawisz, a ostatnie chwile zostaną zapisane jako klip – wcześniej nic nie trafia na dysk.",
   "Clipline ekranınızı arka planda sessizce kaydeder. Bir tuşa basın, son anlar klip olarak kaydedilsin – o ana kadar diske hiçbir şey yazılmaz.",
   "Clipline тихо записывает экран в фоне. Нажмите клавишу – и последние моменты сохранятся как клип. До этого на диск ничего не пишется.",
   "Clipline はバックグラウンドで静かに画面を録画します。キーを押すと直前の瞬間がクリップとして保存され、それまではディスクに何も書き込みません。",
   "Clipline 在后台安静地录制屏幕。按下按键，最近的精彩瞬间就会保存为剪辑——在此之前不会向磁盘写入任何内容。"}},
 {"Clips folder", {"Clips-Ordner","Carpeta de clips","Dossier des clips","Cartella delle clip","Pasta de clipes","Clipsmap","Folder klipów","Klip klasörü","Папка клипов","クリップの保存先","剪辑文件夹"}},
 {"Could not save the clip.", {"Der Clip konnte nicht gespeichert werden.","No se pudo guardar el clip.","Impossible d'enregistrer le clip.","Impossibile salvare la clip.","Não foi possível guardar o clipe.","De clip kon niet worden opgeslagen.","Nie udało się zapisać klipu.","Klip kaydedilemedi.","Не удалось сохранить клип.","クリップを保存できませんでした。","无法保存剪辑。"}},
 {"Dark", {"Dunkel","Oscuro","Sombre","Scuro","Escuro","Donker","Ciemny","Koyu","Тёмная","ダーク","深色"}},
 {"Detail", {"Detail","Detalle","Détail","Dettaglio","Detalhe","Detail","Szczegóły","Ayrıntı","Детализация","詳細度","细节"}},
 {"Estimated RAM", {"Geschätzter RAM","RAM estimada","RAM estimée","RAM stimata","RAM estimada","Geschat RAM-gebruik","Szacowana pamięć RAM","Tahmini RAM","Оценка ОЗУ","推定メモリ使用量","预计内存"}},
 {"Size per clip", {"Größe pro Clip","Tamaño por clip","Taille par clip","Dimensione per clip","Tamanho por clip","Grootte per clip","Rozmiar klipu","Klip başına boyut","Размер клипа","クリップ1本のサイズ","每个片段大小"}},
 {"Update available", {"Update verfügbar","Actualización disponible","Mise à jour disponible","Aggiornamento disponibile","Atualização disponível","Update beschikbaar","Dostępna aktualizacja","Güncelleme mevcut","Доступно обновление","アップデートがあります","有可用更新"}},
 {"%1 %2 is available (you have %3). Update now? The app restarts afterwards.", {"%1 %2 ist verfügbar (du hast %3). Jetzt updaten? Danach startet das Programm neu.","%1 %2 está disponible (tienes %3). ¿Actualizar ahora? La aplicación se reiniciará.","%1 %2 est disponible (vous avez %3). Mettre à jour maintenant ? L'application redémarrera.","%1 %2 è disponibile (hai la %3). Aggiornare ora? L'app si riavvierà.","%1 %2 está disponível (tem a %3). Atualizar agora? A aplicação reinicia a seguir.","%1 %2 is beschikbaar (je hebt %3). Nu bijwerken? Daarna start de app opnieuw.","%1 %2 jest dostępny (masz %3). Zaktualizować teraz? Program uruchomi się ponownie.","%1 %2 mevcut (sizdeki: %3). Şimdi güncellensin mi? Ardından uygulama yeniden başlar.","Доступна версия %1 %2 (у вас %3). Обновить сейчас? Затем программа перезапустится.","%1 %2 が利用できます（現在 %3）。今すぐ更新しますか？更新後に再起動します。","%1 %2 已发布（当前 %3）。现在更新吗？更新后程序将重新启动。"}},
 {"Update now", {"Jetzt updaten","Actualizar ahora","Mettre à jour","Aggiorna ora","Atualizar agora","Nu bijwerken","Aktualizuj teraz","Şimdi güncelle","Обновить сейчас","今すぐ更新","立即更新"}},
 {"Ignore this update", {"Dieses Update ignorieren","Ignorar esta actualización","Ignorer cette mise à jour","Ignora questo aggiornamento","Ignorar esta atualização","Deze update negeren","Pomiń tę aktualizację","Bu güncellemeyi yoksay","Пропустить это обновление","このアップデートを無視","忽略此更新"}},
 {"Later", {"Später","Más tarde","Plus tard","Più tardi","Mais tarde","Later","Później","Sonra","Позже","後で","稍后"}},
 {"Downloading update…", {"Update wird heruntergeladen…","Descargando actualización…","Téléchargement de la mise à jour…","Download dell'aggiornamento…","A transferir atualização…","Update downloaden…","Pobieranie aktualizacji…","Güncelleme indiriliyor…","Загрузка обновления…","アップデートをダウンロード中…","正在下载更新…"}},
 {"The update couldn't be installed automatically. The download page opens instead.", {"Das Update konnte nicht automatisch installiert werden. Stattdessen öffnet sich die Download-Seite.","No se pudo instalar la actualización automáticamente. Se abrirá la página de descarga.","La mise à jour n'a pas pu être installée automatiquement. La page de téléchargement va s'ouvrir.","Impossibile installare l'aggiornamento automaticamente. Si apre la pagina di download.","Não foi possível instalar a atualização automaticamente. Abre-se a página de transferência.","De update kon niet automatisch worden geïnstalleerd. De downloadpagina wordt geopend.","Nie udało się zainstalować aktualizacji automatycznie. Otworzy się strona pobierania.","Güncelleme otomatik yüklenemedi. Bunun yerine indirme sayfası açılıyor.","Не удалось установить обновление автоматически. Откроется страница загрузки.","自動でインストールできませんでした。ダウンロードページを開きます。","无法自动安装更新，将打开下载页面。"}},
 {"You're using the latest version.", {"Du hast die neueste Version.","Tienes la versión más reciente.","Vous avez la dernière version.","Hai la versione più recente.","Tem a versão mais recente.","Je hebt de nieuwste versie.","Masz najnowszą wersję.","En son sürümü kullanıyorsunuz.","У вас последняя версия.","最新バージョンです。","已是最新版本。"}},
 {"Check for updates", {"Nach Updates suchen","Buscar actualizaciones","Rechercher des mises à jour","Controlla aggiornamenti","Procurar atualizações","Controleren op updates","Sprawdź aktualizacje","Güncellemeleri denetle","Проверить обновления","アップデートを確認","检查更新"}},
 {"Frame rate", {"Bildrate","Fotogramas por segundo","Images par seconde","Fotogrammi al secondo","Fotogramas por segundo","Beelden per seconde","Klatki na sekundę","Kare hızı","Частота кадров","フレームレート","帧率"}},
 {"General", {"Allgemein","General","Général","Generale","Geral","Algemeen","Ogólne","Genel","Общие","一般","常规"}},
 {"Global hotkeys are not available here. Bind the command \"clipline --save\" to a key in your system settings.",
  {"Globale Hotkeys sind hier nicht verfügbar. Lege den Befehl \"clipline --save\" in den Systemeinstellungen auf eine Taste.",
   "Aquí no hay atajos globales. Asigna el comando \"clipline --save\" a una tecla en los ajustes del sistema.",
   "Les raccourcis globaux ne sont pas disponibles ici. Associez la commande \"clipline --save\" à une touche dans les réglages du système.",
   "Le scorciatoie globali non sono disponibili qui. Assegna il comando \"clipline --save\" a un tasto nelle impostazioni di sistema.",
   "Os atalhos globais não estão disponíveis aqui. Associe o comando \"clipline --save\" a uma tecla nas definições do sistema.",
   "Globale sneltoetsen zijn hier niet beschikbaar. Koppel de opdracht \"clipline --save\" aan een toets in je systeeminstellingen.",
   "Globalne skróty nie są tu dostępne. Przypisz polecenie \"clipline --save\" do klawisza w ustawieniach systemu.",
   "Genel kısayollar burada kullanılamıyor. \"clipline --save\" komutunu sistem ayarlarından bir tuşa atayın.",
   "Глобальные горячие клавиши здесь недоступны. Назначьте команду \"clipline --save\" на клавишу в настройках системы.",
   "ここではグローバルホットキーを使えません。システム設定でコマンド \"clipline --save\" をキーに割り当ててください。",
   "此处无法使用全局快捷键。请在系统设置中把命令 \"clipline --save\" 绑定到一个按键。"}},
 {"High", {"Hoch","Alta","Élevée","Alta","Alta","Hoog","Wysoka","Yüksek","Высокое","高","高"}},
 {"Hotkey & sound", {"Hotkey & Ton","Tecla y sonido","Raccourci et son","Tasto e audio","Tecla e som","Sneltoets & geluid","Skrót i dźwięk","Kısayol ve ses","Клавиша и звук","ホットキーと音声","快捷键与声音"}},
 {"Language", {"Sprache","Idioma","Langue","Lingua","Idioma","Taal","Język","Dil","Язык","言語","语言"}},
 {"Light", {"Hell","Claro","Clair","Chiaro","Claro","Licht","Jasny","Açık","Светлая","ライト","浅色"}},
 {"Look", {"Aussehen","Apariencia","Apparence","Aspetto","Aspeto","Uiterlijk","Wygląd","Görünüm","Оформление","外観","外观"}},
 {"Make it yours", {"Mach es zu deinem","Hazlo tuyo","À votre image","Rendilo tuo","Torne-o seu","Maak het van jou","Dopasuj do siebie","Kendinize göre ayarlayın","Настройте под себя","自分好みに","打造你的风格"}},
 {"System sound volume", {"Lautstärke Systemton","Volumen del sonido del sistema","Volume du son système","Volume audio di sistema","Volume do som do sistema","Volume systeemgeluid","Głośność dźwięku systemu","Sistem sesi seviyesi","Громкость системного звука","システム音の音量","系统声音音量"}},
 {"Microphone volume", {"Lautstärke Mikrofon","Volumen del micrófono","Volume du micro","Volume del microfono","Volume do microfone","Volume microfoon","Głośność mikrofonu","Mikrofon ses seviyesi","Громкость микрофона","マイクの音量","麦克风音量"}},
 {"Filter out keyboard and mouse clicks (AI noise suppression)", {"Tastatur- und Mausklicks herausfiltern (KI-Rauschunterdrückung)","Filtrar clics de teclado y ratón (supresión de ruido con IA)","Filtrer les clics du clavier et de la souris (réduction de bruit par IA)","Filtra i clic di tastiera e mouse (soppressione del rumore con IA)","Filtrar cliques de teclado e rato (supressão de ruído com IA)","Toetsenbord- en muisklikken wegfilteren (AI-ruisonderdrukking)","Odfiltruj kliknięcia klawiatury i myszy (tłumienie szumów AI)","Klavye ve fare tıklamalarını filtrele (yapay zekâ gürültü bastırma)","Убирать щелчки клавиатуры и мыши (шумоподавление на ИИ)","キーボードとマウスのクリック音を除去（AI ノイズ抑制）","过滤键盘和鼠标点击声（AI 降噪）"}},
 {"Mute key for the microphone", {"Taste zum Stummschalten des Mikrofons","Tecla para silenciar el micrófono","Touche pour couper le micro","Tasto per silenziare il microfono","Tecla para silenciar o microfone","Toets om de microfoon te dempen","Klawisz wyciszania mikrofonu","Mikrofonu sessize alma tuşu","Клавиша отключения микрофона","マイクのミュートキー","麦克风静音键"}},
 {"Press it once and your voice is no longer recorded, press it again to switch the microphone back on. Backspace removes the key.",
  {"Einmal drücken und deine Stimme wird nicht mehr aufgenommen, nochmal drücken und das Mikrofon ist wieder an. Die Rücktaste entfernt die Taste.",
   "Púlsala una vez y tu voz deja de grabarse; vuelve a pulsarla para activar el micrófono. Retroceso quita la tecla.",
   "Appuyez une fois et votre voix n'est plus enregistrée, appuyez à nouveau pour réactiver le micro. Retour arrière supprime la touche.",
   "Premilo una volta e la tua voce non viene più registrata, premilo di nuovo per riattivare il microfono. Backspace rimuove il tasto.",
   "Prima uma vez e a sua voz deixa de ser gravada; prima de novo para voltar a ligar o microfone. Retrocesso remove a tecla.",
   "Druk één keer en je stem wordt niet meer opgenomen, druk nog eens om de microfoon weer aan te zetten. Backspace verwijdert de toets.",
   "Naciśnij raz, a głos przestanie być nagrywany; naciśnij ponownie, aby włączyć mikrofon. Backspace usuwa klawisz.",
   "Bir kez bas, sesin artık kaydedilmez; mikrofonu tekrar açmak için yine bas. Geri tuşu kısayolu kaldırır.",
   "Нажмите один раз — голос перестанет записываться, нажмите снова — микрофон включится. Backspace удаляет клавишу.",
   "1回押すと声が録音されなくなり、もう一度押すとマイクが戻ります。Backspace でキーを削除します。",
   "按一次后不再录制你的声音，再按一次重新打开麦克风。退格键可删除此快捷键。"}},
 {"Mute microphone", {"Mikrofon stummschalten","Silenciar micrófono","Couper le micro","Silenzia microfono","Silenciar microfone","Microfoon dempen","Wycisz mikrofon","Mikrofonu sessize al","Отключить микрофон","マイクをミュート","麦克风静音"}},
 {"Microphone muted", {"Mikrofon stumm","Micrófono silenciado","Micro coupé","Microfono silenziato","Microfone silenciado","Microfoon gedempt","Mikrofon wyciszony","Mikrofon sessiz","Микрофон выключен","マイクはミュート中","麦克风已静音"}},
 {"Microphone on", {"Mikrofon an","Micrófono activado","Micro activé","Microfono attivo","Microfone ligado","Microfoon aan","Mikrofon włączony","Mikrofon açık","Микрофон включён","マイク オン","麦克风已开启"}},
 {"Your voice is not recorded until you press the key again.", {"Deine Stimme wird nicht aufgenommen, bis du die Taste nochmal drückst.","Tu voz no se graba hasta que vuelvas a pulsar la tecla.","Votre voix n'est pas enregistrée tant que vous n'appuyez pas à nouveau.","La tua voce non viene registrata finché non premi di nuovo il tasto.","A sua voz não é gravada até premir a tecla novamente.","Je stem wordt niet opgenomen tot je de toets opnieuw indrukt.","Głos nie jest nagrywany, dopóki nie naciśniesz klawisza ponownie.","Tuşa tekrar basana kadar sesin kaydedilmez.","Голос не записывается, пока вы снова не нажмёте клавишу.","もう一度キーを押すまで声は録音されません。","在你再次按下该键之前，不会录制你的声音。"}},
 {"Screen sharing was cancelled.", {"Die Bildschirmfreigabe wurde abgebrochen.","Se canceló el uso compartido de pantalla.","Le partage d'écran a été annulé.","La condivisione dello schermo è stata annullata.","A partilha de ecrã foi cancelada.","Scherm delen is geannuleerd.","Udostępnianie ekranu zostało anulowane.","Ekran paylaşımı iptal edildi.","Демонстрация экрана отменена.","画面共有がキャンセルされました。","屏幕共享已取消。"}},
 {"Screen sharing is not available.", {"Bildschirmfreigabe ist nicht verfügbar.","El uso compartido de pantalla no está disponible.","Le partage d'écran n'est pas disponible.","La condivisione dello schermo non è disponibile.","A partilha de ecrã não está disponível.","Scherm delen is niet beschikbaar.","Udostępnianie ekranu jest niedostępne.","Ekran paylaşımı kullanılamıyor.","Демонстрация экрана недоступна.","画面共有は利用できません。","屏幕共享不可用。"}},
 {"No screen was shared.", {"Es wurde kein Bildschirm freigegeben.","No se compartió ninguna pantalla.","Aucun écran n'a été partagé.","Nessuno schermo è stato condiviso.","Nenhum ecrã foi partilhado.","Er is geen scherm gedeeld.","Nie udostępniono żadnego ekranu.","Hiçbir ekran paylaşılmadı.","Экран не был выбран.","画面が共有されませんでした。","未共享任何屏幕。"}},
 {"PipeWire could not be opened.", {"PipeWire konnte nicht geöffnet werden.","No se pudo abrir PipeWire.","Impossible d'ouvrir PipeWire.","Impossibile aprire PipeWire.","Não foi possível abrir o PipeWire.","PipeWire kon niet worden geopend.","Nie można otworzyć PipeWire.","PipeWire açılamadı.","Не удалось открыть PipeWire.","PipeWire を開けませんでした。","无法打开 PipeWire。"}},
 {"Clipline can't record the screen:", {"Clipline kann den Bildschirm nicht aufnehmen:","Clipline no puede grabar la pantalla:","Clipline ne peut pas enregistrer l'écran :","Clipline non può registrare lo schermo:","O Clipline não consegue gravar o ecrã:","Clipline kan het scherm niet opnemen:","Clipline nie może nagrywać ekranu:","Clipline ekranı kaydedemiyor:","Clipline не может записывать экран:","Clipline は画面を録画できません:","Clipline 无法录制屏幕："}},
 {"Recording under Wayland needs GStreamer with the PipeWire plugin. Install it, e.g. \"sudo pacman -S gst-plugin-pipewire gst-plugins-base\" or \"sudo apt install gstreamer1.0-pipewire gstreamer1.0-tools\", then restart Clipline.",
  {"Für die Aufnahme unter Wayland braucht Clipline GStreamer mit PipeWire-Plugin. Installiere es, z. B. mit \"sudo pacman -S gst-plugin-pipewire gst-plugins-base\" oder \"sudo apt install gstreamer1.0-pipewire gstreamer1.0-tools\", und starte Clipline neu.",
   "Para grabar en Wayland, Clipline necesita GStreamer con el plugin de PipeWire. Instálalo, p. ej. \"sudo pacman -S gst-plugin-pipewire gst-plugins-base\" o \"sudo apt install gstreamer1.0-pipewire gstreamer1.0-tools\", y reinicia Clipline.",
   "Sous Wayland, Clipline a besoin de GStreamer avec le plugin PipeWire. Installez-le, p. ex. \"sudo pacman -S gst-plugin-pipewire gst-plugins-base\" ou \"sudo apt install gstreamer1.0-pipewire gstreamer1.0-tools\", puis redémarrez Clipline.",
   "Per registrare su Wayland Clipline richiede GStreamer con il plugin PipeWire. Installalo, ad es. \"sudo pacman -S gst-plugin-pipewire gst-plugins-base\" o \"sudo apt install gstreamer1.0-pipewire gstreamer1.0-tools\", poi riavvia Clipline.",
   "Para gravar no Wayland, o Clipline precisa do GStreamer com o plugin PipeWire. Instale-o, p. ex. \"sudo pacman -S gst-plugin-pipewire gst-plugins-base\" ou \"sudo apt install gstreamer1.0-pipewire gstreamer1.0-tools\", e reinicie o Clipline.",
   "Opnemen onder Wayland vereist GStreamer met de PipeWire-plugin. Installeer die, bijv. \"sudo pacman -S gst-plugin-pipewire gst-plugins-base\" of \"sudo apt install gstreamer1.0-pipewire gstreamer1.0-tools\", en start Clipline opnieuw.",
   "Nagrywanie pod Wayland wymaga GStreamera z wtyczką PipeWire. Zainstaluj go, np. \"sudo pacman -S gst-plugin-pipewire gst-plugins-base\" lub \"sudo apt install gstreamer1.0-pipewire gstreamer1.0-tools\", i uruchom Clipline ponownie.",
   "Wayland altında kayıt için PipeWire eklentili GStreamer gerekir. Örneğin \"sudo pacman -S gst-plugin-pipewire gst-plugins-base\" veya \"sudo apt install gstreamer1.0-pipewire gstreamer1.0-tools\" ile kurun ve Clipline'ı yeniden başlatın.",
   "Для записи под Wayland нужен GStreamer с плагином PipeWire. Установите его, например \"sudo pacman -S gst-plugin-pipewire gst-plugins-base\" или \"sudo apt install gstreamer1.0-pipewire gstreamer1.0-tools\", и перезапустите Clipline.",
   "Wayland で録画するには PipeWire プラグイン付きの GStreamer が必要です。例: \"sudo pacman -S gst-plugin-pipewire gst-plugins-base\" または \"sudo apt install gstreamer1.0-pipewire gstreamer1.0-tools\" でインストールし、Clipline を再起動してください。",
   "在 Wayland 下录制需要带 PipeWire 插件的 GStreamer。请安装，例如 \"sudo pacman -S gst-plugin-pipewire gst-plugins-base\" 或 \"sudo apt install gstreamer1.0-pipewire gstreamer1.0-tools\"，然后重启 Clipline。"}},
 {"Microphone sensitivity", {"Mikrofon-Empfindlichkeit","Sensibilidad del micrófono","Sensibilité du micro","Sensibilità del microfono","Sensibilidade do microfone","Microfoongevoeligheid","Czułość mikrofonu","Mikrofon hassasiyeti","Чувствительность микрофона","マイクの感度","麦克风灵敏度"}},
 {"Off", {"Aus","Desactivado","Désactivé","Disattivato","Desligado","Uit","Wyłączone","Kapalı","Выкл.","オフ","关闭"}},
 {"Record everything", {"Alles aufnehmen","Grabar todo","Tout enregistrer","Registra tutto","Gravar tudo","Alles opnemen","Nagrywaj wszystko","Her şeyi kaydet","Записывать всё","すべて録音","全部录制"}},
 {"Only loud voices", {"Nur laute Stimmen","Solo voces fuertes","Seulement les voix fortes","Solo voci forti","Só vozes altas","Alleen luide stemmen","Tylko głośne głosy","Yalnızca yüksek sesler","Только громкие голоса","大きな声のみ","仅较大的声音"}},
 {"Talk normally: the bar should turn green when you speak and stay grey when you are quiet. Everything below the line is muted, so background noise stays out of your clips.",
  {"Sprich ganz normal: Der Balken sollte grün werden, wenn du redest, und grau bleiben, wenn du still bist. Alles unter der Linie wird stummgeschaltet – so bleiben Hintergrundgeräusche aus deinen Clips.",
   "Habla con normalidad: la barra debería ponerse verde al hablar y quedarse gris en silencio. Todo lo que quede por debajo de la línea se silencia, así el ruido de fondo no entra en tus clips.",
   "Parlez normalement : la barre doit devenir verte quand vous parlez et rester grise quand vous vous taisez. Tout ce qui est sous la ligne est coupé, le bruit de fond reste hors de vos clips.",
   "Parla normalmente: la barra dovrebbe diventare verde quando parli e restare grigia quando stai zitto. Tutto ciò che è sotto la linea viene silenziato, così il rumore di fondo resta fuori dalle clip.",
   "Fale normalmente: a barra deve ficar verde quando fala e cinzenta quando está em silêncio. Tudo abaixo da linha é silenciado, assim o ruído de fundo fica fora dos clipes.",
   "Praat gewoon: de balk hoort groen te worden als je praat en grijs te blijven als je stil bent. Alles onder de lijn wordt gedempt, zo blijft achtergrondgeluid uit je clips.",
   "Mów normalnie: pasek powinien zmieniać się na zielony, gdy mówisz, i pozostać szary, gdy milczysz. Wszystko poniżej linii jest wyciszane, więc hałas w tle nie trafia do klipów.",
   "Normal konuş: konuşurken çubuk yeşile dönmeli, sessizken gri kalmalı. Çizginin altındaki her şey susturulur, böylece arka plan gürültüsü kliplere girmez.",
   "Говорите как обычно: полоса должна становиться зелёной, когда вы говорите, и оставаться серой в тишине. Всё ниже линии заглушается, так фоновый шум не попадает в клипы.",
   "普段どおり話してください。話すとバーが緑になり、黙っているときは灰色のままになるのが目安です。線より下の音はミュートされ、周囲の雑音はクリップに入りません。",
   "正常说话：说话时横条应变绿，安静时保持灰色。线以下的声音会被静音，背景噪音就不会进入剪辑。"}},
 {"Microphone", {"Mikrofon","Micrófono","Microphone","Microfono","Microfone","Microfoon","Mikrofon","Mikrofon","Микрофон","マイク","麦克风"}},
 {"Mode", {"Modus","Modo","Mode","Modalità","Modo","Modus","Tryb","Mod","Режим","モード","模式"}},
 {"Move \"%1\" to the trash?", {"\"%1\" in den Papierkorb verschieben?","¿Mover \"%1\" a la papelera?","Mettre \"%1\" à la corbeille ?","Spostare \"%1\" nel cestino?","Mover \"%1\" para o lixo?","\"%1\" naar de prullenbak verplaatsen?","Przenieść \"%1\" do kosza?","\"%1\" çöp kutusuna taşınsın mı?","Переместить \"%1\" в корзину?","\"%1\" をごみ箱に移動しますか？","将 \"%1\" 移到回收站？"}},
 {"Move to trash", {"In den Papierkorb","Mover a la papelera","Mettre à la corbeille","Sposta nel cestino","Mover para o lixo","Naar prullenbak","Przenieś do kosza","Çöp kutusuna taşı","В корзину","ごみ箱に移動","移到回收站"}},
 {"Native", {"Nativ","Nativa","Native","Nativa","Nativa","Oorspronkelijk","Natywna","Yerel","Исходное","ネイティブ","原生"}},
 {"New name:", {"Neuer Name:","Nuevo nombre:","Nouveau nom :","Nuovo nome:","Novo nome:","Nieuwe naam:","Nowa nazwa:","Yeni ad:","Новое имя:","新しい名前：","新名称："}},
 {"Next", {"Weiter","Siguiente","Suivant","Avanti","Seguinte","Volgende","Dalej","İleri","Далее","次へ","下一步"}},
 {"No clips yet", {"Noch keine Clips","Aún no hay clips","Pas encore de clips","Ancora nessuna clip","Ainda sem clipes","Nog geen clips","Brak klipów","Henüz klip yok","Клипов пока нет","まだクリップがありません","还没有剪辑"}},
 {"None", {"Keins","Ninguno","Aucun","Nessuno","Nenhum","Geen","Brak","Yok","Нет","なし","无"}},
 {"Not available on this system", {"Auf diesem System nicht verfügbar","No disponible en este sistema","Non disponible sur ce système","Non disponibile su questo sistema","Não disponível neste sistema","Niet beschikbaar op dit systeem","Niedostępne w tym systemie","Bu sistemde kullanılamıyor","Недоступно в этой системе","このシステムでは使用できません","此系统不可用"}},
 {"Nothing recorded yet.", {"Noch nichts aufgenommen.","Aún no se ha grabado nada.","Rien n'a encore été enregistré.","Non è ancora stato registrato nulla.","Ainda não foi gravado nada.","Nog niets opgenomen.","Jeszcze nic nie nagrano.","Henüz bir şey kaydedilmedi.","Пока ничего не записано.","まだ何も録画されていません。","还没有录制任何内容。"}},
 {"Open Clipline", {"Clipline öffnen","Abrir Clipline","Ouvrir Clipline","Apri Clipline","Abrir Clipline","Clipline openen","Otwórz Clipline","Clipline'ı aç","Открыть Clipline","Clipline を開く","打开 Clipline"}},
 {"Open folder", {"Ordner öffnen","Abrir carpeta","Ouvrir le dossier","Apri cartella","Abrir pasta","Map openen","Otwórz folder","Klasörü aç","Открыть папку","フォルダーを開く","打开文件夹"}},
 {"Pause recording", {"Aufnahme pausieren","Pausar grabación","Suspendre l'enregistrement","Metti in pausa la registrazione","Pausar gravação","Opname pauzeren","Wstrzymaj nagrywanie","Kaydı duraklat","Приостановить запись","録画を一時停止","暂停录制"}},
 {"Paused", {"Pausiert","En pausa","En pause","In pausa","Em pausa","Gepauzeerd","Wstrzymano","Duraklatıldı","Приостановлено","一時停止中","已暂停"}},
 {"Play a sound when a clip is saved", {"Ton abspielen, wenn ein Clip gespeichert wird","Reproducir un sonido al guardar un clip","Jouer un son quand un clip est enregistré","Riproduci un suono quando una clip viene salvata","Tocar um som ao guardar um clipe","Geluid afspelen als een clip is opgeslagen","Odtwórz dźwięk po zapisaniu klipu","Klip kaydedilince ses çal","Звук при сохранении клипа","クリップ保存時にサウンドを鳴らす","保存剪辑时播放提示音"}},
 {"Play", {"Abspielen","Reproducir","Lire","Riproduci","Reproduzir","Afspelen","Odtwórz","Oynat","Воспроизвести","再生","播放"}},
 {"Press %1 and the last %2 are saved here.", {"Drücke %1 und die letzten %2 landen hier.","Pulsa %1 y los últimos %2 se guardan aquí.","Appuyez sur %1 et les %2 derniers arrivent ici.","Premi %1 e gli ultimi %2 vengono salvati qui.","Prima %1 e os últimos %2 ficam guardados aqui.","Druk op %1 en de laatste %2 komen hier terecht.","Naciśnij %1, a ostatnie %2 trafią tutaj.","%1 tuşuna basın, son %2 buraya kaydedilir.","Нажмите %1 – последние %2 сохранятся здесь.","%1 を押すと直前の %2 がここに保存されます。","按 %1，最近的 %2 会保存到这里。"}},
 {"Press keys…", {"Tasten drücken…","Pulsa las teclas…","Appuyez sur les touches…","Premi i tasti…","Prima as teclas…","Druk op toetsen…","Naciśnij klawisze…","Tuşlara basın…","Нажмите клавиши…","キーを押してください…","请按键…"}},
 {"Quit", {"Beenden","Salir","Quitter","Esci","Sair","Afsluiten","Zakończ","Çıkış","Выход","終了","退出"}},
 {"Recording failed to start:", {"Die Aufnahme konnte nicht starten:","No se pudo iniciar la grabación:","L'enregistrement n'a pas pu démarrer :","Impossibile avviare la registrazione:","Não foi possível iniciar a gravação:","De opname kon niet starten:","Nie udało się rozpocząć nagrywania:","Kayıt başlatılamadı:","Не удалось начать запись:","録画を開始できませんでした：","无法开始录制："}},
 {"Recording", {"Nimmt auf","Grabando","Enregistrement","In registrazione","A gravar","Neemt op","Nagrywanie","Kaydediliyor","Идёт запись","録画中","正在录制"}},
 {"Rename clip", {"Clip umbenennen","Renombrar clip","Renommer le clip","Rinomina clip","Mudar o nome do clipe","Clip hernoemen","Zmień nazwę klipu","Klibi yeniden adlandır","Переименовать клип","クリップの名前を変更","重命名剪辑"}},
 {"Rename…", {"Umbenennen…","Renombrar…","Renommer…","Rinomina…","Mudar o nome…","Hernoemen…","Zmień nazwę…","Yeniden adlandır…","Переименовать…","名前を変更…","重命名…"}},
 {"Resolution", {"Auflösung","Resolución","Résolution","Risoluzione","Resolução","Resolutie","Rozdzielczość","Çözünürlük","Разрешение","解像度","分辨率"}},
 {"Resume recording", {"Aufnahme fortsetzen","Reanudar grabación","Reprendre l'enregistrement","Riprendi la registrazione","Retomar gravação","Opname hervatten","Wznów nagrywanie","Kayda devam et","Продолжить запись","録画を再開","继续录制"}},
 {"Save clip", {"Clip speichern","Guardar clip","Enregistrer le clip","Salva clip","Guardar clipe","Clip opslaan","Zapisz klip","Klibi kaydet","Сохранить клип","クリップを保存","保存剪辑"}},
 {"Save", {"Speichern","Guardar","Enregistrer","Salva","Guardar","Opslaan","Zapisz","Kaydet","Сохранить","保存","保存"}},
 {"Screen", {"Bildschirm","Pantalla","Écran","Schermo","Ecrã","Scherm","Ekran","Ekran","Экран","画面","屏幕"}},
 {"Settings", {"Einstellungen","Ajustes","Paramètres","Impostazioni","Definições","Instellingen","Ustawienia","Ayarlar","Настройки","設定","设置"}},
 {"Show in folder", {"Im Ordner zeigen","Mostrar en la carpeta","Afficher dans le dossier","Mostra nella cartella","Mostrar na pasta","Tonen in map","Pokaż w folderze","Klasörde göster","Показать в папке","フォルダーで表示","在文件夹中显示"}},
 {"Small", {"Klein","Pequeña","Petite","Piccola","Pequena","Klein","Mała","Küçük","Малое","小","小"}},
 {"Sound", {"Ton","Sonido","Son","Audio","Som","Geluid","Dźwięk","Ses","Звук","音声","声音"}},
 {"Start recording", {"Aufnahme starten","Empezar a grabar","Commencer l'enregistrement","Avvia la registrazione","Começar a gravar","Opname starten","Rozpocznij nagrywanie","Kaydı başlat","Начать запись","録画を開始","开始录制"}},
 {"Start with the computer (runs in the background)", {"Mit dem Computer starten (läuft im Hintergrund)","Iniciar con el equipo (en segundo plano)","Démarrer avec l'ordinateur (en arrière-plan)","Avvia con il computer (in background)","Iniciar com o computador (em segundo plano)","Starten met de computer (op de achtergrond)","Uruchamiaj z komputerem (działa w tle)","Bilgisayarla başlat (arka planda çalışır)","Запускать вместе с компьютером (в фоне)","コンピューターと一緒に起動（バックグラウンド）","开机自动启动（后台运行）"}},
 {"Starting…", {"Startet…","Iniciando…","Démarrage…","Avvio…","A iniciar…","Starten…","Uruchamianie…","Başlatılıyor…","Запуск…","起動中…","正在启动…"}},
 {"System sound", {"Systemton","Sonido del sistema","Son du système","Audio di sistema","Som do sistema","Systeemgeluid","Dźwięk systemowy","Sistem sesi","Системный звук","システム音声","系统声音"}},
 {"System", {"System","Sistema","Système","Sistema","Sistema","Systeem","Systemowy","Sistem","Системный","システム","跟随系统"}},
 {"The audio device could not be opened. Recording continues without sound.",
  {"Das Audiogerät konnte nicht geöffnet werden. Die Aufnahme läuft ohne Ton weiter.",
   "No se pudo abrir el dispositivo de audio. La grabación continúa sin sonido.",
   "Impossible d'ouvrir le périphérique audio. L'enregistrement continue sans son.",
   "Impossibile aprire il dispositivo audio. La registrazione continua senza audio.",
   "Não foi possível abrir o dispositivo de áudio. A gravação continua sem som.",
   "Het audioapparaat kon niet worden geopend. De opname gaat verder zonder geluid.",
   "Nie udało się otworzyć urządzenia audio. Nagrywanie trwa bez dźwięku.",
   "Ses aygıtı açılamadı. Kayıt sessiz devam ediyor.",
   "Не удалось открыть аудиоустройство. Запись продолжается без звука.",
   "オーディオデバイスを開けませんでした。音声なしで録画を続けます。",
   "无法打开音频设备。录制将在无声音的情况下继续。"}},
 {"The hotkey %1 is already used by another program. Choose a different one in the settings.",
  {"Der Hotkey %1 wird schon von einem anderen Programm benutzt. Wähle in den Einstellungen einen anderen.",
   "Otro programa ya usa la tecla %1. Elige otra en los ajustes.",
   "Le raccourci %1 est déjà utilisé par un autre programme. Choisissez-en un autre dans les paramètres.",
   "Il tasto %1 è già usato da un altro programma. Scegline un altro nelle impostazioni.",
   "A tecla %1 já é usada por outro programa. Escolha outra nas definições.",
   "De sneltoets %1 wordt al door een ander programma gebruikt. Kies een andere in de instellingen.",
   "Skrót %1 jest już używany przez inny program. Wybierz inny w ustawieniach.",
   "%1 kısayolu başka bir program tarafından kullanılıyor. Ayarlardan farklı bir tane seçin.",
   "Сочетание %1 уже занято другой программой. Выберите другое в настройках.",
   "ホットキー %1 は他のプログラムで使用中です。設定で別のキーを選んでください。",
   "快捷键 %1 已被其他程序占用。请在设置中选择其他按键。"}},
 {"Welcome to Clipline", {"Willkommen bei Clipline","Bienvenido a Clipline","Bienvenue dans Clipline","Benvenuto in Clipline","Bem-vindo ao Clipline","Welkom bij Clipline","Witaj w Clipline","Clipline'a hoş geldiniz","Добро пожаловать в Clipline","Clipline へようこそ","欢迎使用 Clipline"}},
 {"Where should your clips go?", {"Wo sollen deine Clips landen?","¿Dónde se guardan tus clips?","Où enregistrer vos clips ?","Dove salvare le clip?","Onde guardar os seus clipes?","Waar moeten je clips komen?","Gdzie zapisywać klipy?","Klipleriniz nereye kaydedilsin?","Куда сохранять клипы?","クリップの保存先は？","剪辑保存到哪里？"}},
 {"Your clip key", {"Deine Clip-Taste","Tu tecla de clip","Votre touche de clip","Il tuo tasto per le clip","A sua tecla de clipe","Jouw cliptoets","Twój klawisz klipu","Klip tuşunuz","Ваша клавиша клипа","クリップキー","你的剪辑快捷键"}},
 {"buffer", {"Puffer","búfer","tampon","buffer","buffer","buffer","bufor","arabellek","буфер","バッファ","缓冲"}},
 {"last %1", {"letzte %1","últimos %1","dernières %1","ultimi %1","últimos %1","laatste %1","ostatnie %1","son %1","последние %1","直前 %1","最近 %1"}},
};

int g_lang = 0;  // 0 = Englisch

const QHash<QString, const Entry*>& index() {
    static const QHash<QString, const Entry*> h = [] {
        QHash<QString, const Entry*> m;
        for (const Entry& e : kTable) m.insert(QString::fromUtf8(e.en), &e);
        return m;
    }();
    return h;
}
}  // namespace

QStringList languageCodes() {
    QStringList l;
    for (const char* c : kCodes) l << c;
    return l;
}

QStringList languageNames() {
    QStringList l;
    for (const char* c : kNames) l << QString::fromUtf8(c);
    return l;
}

void setLanguage(const QString& code) {
    const QString c = code.isEmpty() ? QLocale::system().name().left(2) : code;
    const int i = languageCodes().indexOf(c);
    g_lang = i < 0 ? 0 : i;
}

QString language() { return kCodes[g_lang]; }

QString L(const char* english) {
    const QString key = QString::fromUtf8(english);
    if (g_lang == 0) return key;
    auto it = index().constFind(key);
    return it == index().constEnd() ? key : QString::fromUtf8((*it)->t[g_lang - 1]);
}
