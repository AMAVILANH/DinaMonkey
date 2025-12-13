clc; clear; close all;

% Parámetros del material y la geometría
E = 71e9;         % Módulo de Young del aluminio 7075 [Pa]
L = 0.2;          % Longitud del cilindro [m]
F = linspace(100, 10000, 100);  % Carga aplicada [N]
D = 0.01:0.01:0.05;             % Diámetros [m]

% Figura
figure; hold on; grid on;
colors = lines(length(D));

for i = 1:length(D)
    A = pi * (D(i)/2)^2;          % Área transversal
    deltaL = (F .* L) ./ (A * E); % Deformación real [m]
    plot(F, deltaL, 'LineWidth', 2, 'Color', colors(i,:), ...
        'DisplayName', sprintf('D = %.2f m', D(i)));
end

xlabel('Carga [N]');
ylabel('Deformación real \DeltaL [m]');
title('Curvas de Deformación Real vs Carga - Aluminio 7075');
legend('Location', 'northwest');
grid on;
