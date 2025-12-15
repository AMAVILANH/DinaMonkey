clc; clear; close all;

%% =======================
%  DATOS EXPERIMENTALES
% =======================

data_loading = struct( ...
    'N0',    [-28 -9 -11 -21 -20 0 2 6 -10 -27 -41 -40 -35 -39 -36 -38 -41 -46 -42 -29 -15 -6 -22 -23 -24 -36 -27], ...
    'N500',  [482 494 479 491 482 477 490 503 489 496 486 465 477 488 481 472 488 495 496 496 487 506 493 492 477 469 468 472 484 478], ...
    'N1000', [953 950 969 979 983 982 980 959 950 950 951 942 932 930 935 944 953 951 952 946 934 942 954 943 932 930 931 922 943 955], ...
    'N2000', [1752 1756 1756 1755 1759 1753 1767 1769 1757 1766 1780 1771 1792 1814 1807 1799 1789 1777 1783 1763 1757 1770 1763 1754 1762 1767 1772 1760 1748 1751], ...
    'N3000', [2674 2672 2668 2673 2667 2664 2663 2653 2655 2656 2642 2630 2613 2617 2622 2625 2625 2632 2640 2624 2607 2610 2631 2626 2625 2622 2637 2643 2651 2661], ...
    'N4000', [3515 3507 3493 3488 3499 3498 3490 3497 3486 3476 3465 3462 3467 3459 3443 3443 3460 3476 3477 3486 3491 3496 3492 3512 3528 3530 3517 3512 3504 3508] ...
);

loads  = [0 500 1000 2000 3000 4000];
fields = fieldnames(data_loading);

%% =======================
%  ESTADÍSTICAS DE CARGA
% =======================

meanL = zeros(numel(loads),1);
stdL  = zeros(numel(loads),1);
nsamp = zeros(numel(loads),1);

for i = 1:numel(loads)
    v = data_loading.(fields{i});
    meanL(i) = mean(v);
    stdL(i)  = std(v,1);        % STD muestral
    nsamp(i) = numel(v);
end

disp('=== LOADING STATS ===');
disp(table(loads(:), nsamp(:), meanL(:), stdL(:), ...
    'VariableNames', {'Load_N','N_samples','Mean_raw','Std_raw'}));

%% =======================
%  SENSIBILIDAD POR SEGMENTO
% =======================

seg_names  = strings(numel(loads)-1,1);
seg_counts = zeros(numel(loads)-1,1);

for i = 1:numel(loads)-1
    seg_names(i)  = sprintf('%d-%d', loads(i), loads(i+1));
    seg_counts(i) = (meanL(i+1) - meanL(i)) / (loads(i+1) - loads(i));
end

disp('=== SENSIBILIDAD POR SEGMENTO ===');
disp(table(seg_names, seg_counts, 1./seg_counts, ...
    'VariableNames', {'Segment','Counts_per_N','N_per_count'}));

%% =======================
%  PRECISIÓN (STD EN NEWTONS)
% =======================

stdN = zeros(numel(loads),1);

for i = 1:numel(loads)
    if i < numel(loads)
        sens = seg_counts(i);
    else
        sens = seg_counts(end);
    end
    stdN(i) = stdL(i) / sens;
end

disp('=== PRECISIÓN ===');
disp(table(loads(:), stdL(:), stdN(:), ...
    'VariableNames', {'Load_N','Std_counts','Std_N'}));

%% =======================
%  REGRESIÓN LINEAL GLOBAL
% =======================

p = polyfit(loads(:), meanL(:), 1);
slope  = p(1);
offset = p(2);

mean_fit = polyval(p, loads(:));

R2 = 1 - sum((meanL - mean_fit).^2) / sum((meanL - mean(meanL)).^2);

fprintf('\nCalibración global:\n');
fprintf('Counts = %.6f · N + %.3f\n', slope, offset);
fprintf('R² = %.6f\n', R2);

%% =======================
%  GRÁFICAS (SOLO PLOT)
% =======================

opengl software;
set(0,'DefaultFigureVisible','on');

% -------- Curva de calibración --------
figure('Color','w');
errorbar(loads(:), meanL(:), stdL(:), 'o-', ...
    'LineWidth',1.5,'MarkerSize',6,'CapSize',8);
hold on;
plot(loads(:), mean_fit, '--r', 'LineWidth',1.5);
grid on;
xlabel('Carga [N]');
ylabel('Lectura [counts]');
title('Curva de calibración del dinamómetro');
legend('Media ± STD','Ajuste lineal','Location','northwest');
drawnow;

% -------- Precisión --------
figure('Color','w');
bar(loads(:), stdN(:));
grid on;
xlabel('Carga [N]');
ylabel('STD [N]');
title('Precisión (desviación estándar)');
drawnow;
